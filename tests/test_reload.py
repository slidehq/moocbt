#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only

#
# Copyright (C) 2026 Project Orca Inc.
#

import os
import unittest

import moocbt
import util
from devicetestcase import DeviceTestCase


class TestReload(DeviceTestCase):
    def setUp(self):
        self.device = "/dev/loop0"
        self.mount = "/tmp/moocbt"
        self.cow_file = "cow.snap"
        self.cow_full_path = "{}/{}".format(self.mount, self.cow_file)
        # reload takes the cow path relative to the volume root
        self.cow_reload_path = "/{}".format(self.cow_file)
        self.minor = 1
        self.snap_device = "/dev/moocbt{}".format(self.minor)

    def _ensure_mounted(self):
        # the lifecycle tests remount to trigger the mount hook,
        # so only mount here if an earlier failure left it unmounted.
        if not os.path.ismount(self.mount):
            util.mount(self.device, self.mount)

    def _track_then_teardown(self, to_incremental):
        # Like _setup_then_teardown, but writes known data first so there is a
        # non-zero changed-block count persisted into the cow file. Returns the
        # count observed just before the device is torn down.
        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)
        if to_incremental:
            self.assertEqual(moocbt.transition_to_incremental(self.minor), 0)

        with open("{}/tracked".format(self.mount), "w") as f:
            f.write("Z" * 200000)  # spans many 4K blocks
        os.sync()

        pre = moocbt.info(self.minor)["nr_changed_blocks"]
        self.assertGreater(pre, 0)

        # Unmount syncs and closes the cow file (persisting the tracking data),
        # then destroy leaves the cow file on disk, as across a reboot.
        util.unmount(self.mount)
        self.addCleanup(self._ensure_mounted)
        self.assertEqual(moocbt.destroy(self.minor), 0)
        return pre

    def test_reload_incremental_preserves_tracking(self):
        # The whole point of reload: changed-block tracking survives a module
        # unload / reboot, so the next incremental backup copies the right data.
        pre = self._track_then_teardown(to_incremental=True)

        self.assertEqual(moocbt.reload_incremental(self.minor, self.device, self.cow_reload_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)
        self.assertEqual(moocbt.info(self.minor)["state"], 4)  # UNVERIFIED

        util.mount(self.device, self.mount)
        util.settle()

        info = moocbt.info(self.minor)
        self.assertEqual(info["state"], 2)  # ACTIVE (incremental)
        self.assertEqual(info["error"], 0)
        self.assertGreaterEqual(info["nr_changed_blocks"], pre,
                                "changed-block count was lost across reload")

    def test_reload_snapshot_preserves_tracking(self):
        pre = self._track_then_teardown(to_incremental=False)

        self.assertEqual(moocbt.reload_snapshot(self.minor, self.device, self.cow_reload_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)
        self.assertEqual(moocbt.info(self.minor)["state"], 5)  # UNVERIFIED | SNAPSHOT

        # The snapshot device is not created until the device is verified
        self.assertFalse(os.path.exists(self.snap_device))

        util.mount(self.device, self.mount)
        util.settle()

        info = moocbt.info(self.minor)
        self.assertEqual(info["state"], 3)  # ACTIVE | SNAPSHOT
        self.assertEqual(info["error"], 0)
        self.assertGreaterEqual(info["nr_changed_blocks"], pre,
                                "changed-block count was lost across reload")
        # Recreates the snapshot device
        self.assertTrue(os.path.exists(self.snap_device))


if __name__ == "__main__":
    unittest.main()

