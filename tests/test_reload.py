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

    def _setup_then_teardown(self, to_incremental):
        # Setup tracking, creating a valid cow file on the volume, then destroy the tracer.
        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)
        if to_incremental:
            self.assertEqual(moocbt.transition_to_incremental(self.minor), 0)
        # Unmounting transitions the snap device to dormant, which syncs and
        # closes the cow file.
        util.unmount(self.mount)
        self.addCleanup(self._ensure_mounted)
        # Destroy preserves the cow file on disk, similar to the module being
        # unloaded across a reboot with tracking data intact.
        self.assertEqual(moocbt.destroy(self.minor), 0)

    def test_reload_incremental_sets_unverified(self):
        self._setup_then_teardown(to_incremental=True)

        self.assertEqual(moocbt.reload_incremental(self.minor, self.device, self.cow_reload_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)

        info = moocbt.info(self.minor)
        self.assertIsNotNone(info)
        self.assertEqual(info["state"], 4)  # UNVERIFIED (incremental)
        self.assertEqual(info["error"], 0)

    def test_reload_snapshot_sets_unverified(self):
        self._setup_then_teardown(to_incremental=False)

        self.assertEqual(moocbt.reload_snapshot(self.minor, self.device, self.cow_reload_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)

        info = moocbt.info(self.minor)
        self.assertIsNotNone(info)
        self.assertEqual(info["state"], 5)  # UNVERIFIED | SNAPSHOT
        self.assertEqual(info["error"], 0)
        # The snapshot device is not created until the device is verified
        self.assertFalse(os.path.exists(self.snap_device))

    def test_reload_incremental_mount_activates(self):
        self._setup_then_teardown(to_incremental=True)

        self.assertEqual(moocbt.reload_incremental(self.minor, self.device, self.cow_reload_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)
        self.assertEqual(moocbt.info(self.minor)["state"], 4)  # UNVERIFIED

        # Mounting the tracked volume fires the mount hook, which verifies the
        # cow file and transitions the device from unverified to active.
        util.mount(self.device, self.mount)
        util.settle()

        info = moocbt.info(self.minor)
        self.assertEqual(info["state"], 2)  # ACTIVE (incremental)
        self.assertEqual(info["error"], 0)

    def test_reload_snapshot_mount_activates(self):
        self._setup_then_teardown(to_incremental=False)

        self.assertEqual(moocbt.reload_snapshot(self.minor, self.device, self.cow_reload_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)
        self.assertEqual(moocbt.info(self.minor)["state"], 5)  # UNVERIFIED | SNAPSHOT

        util.mount(self.device, self.mount)
        util.settle()

        info = moocbt.info(self.minor)
        self.assertEqual(info["state"], 3)  # ACTIVE | SNAPSHOT
        self.assertEqual(info["error"], 0)
        # Verification recreates the snapshot block device.
        self.assertTrue(os.path.exists(self.snap_device))


if __name__ == "__main__":
    unittest.main()

