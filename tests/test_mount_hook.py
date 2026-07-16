#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only

#
# Copyright (C) 2026 Project Orca Inc.
#

import errno
import os
import unittest

import moocbt
import util
from devicetestcase import DeviceTestCase

DORMANT_INCREMENTAL = 0
DORMANT_SNAPSHOT = 1
ACTIVE_INCREMENTAL = 2
ACTIVE_SNAPSHOT = 3

class TestMountHook(DeviceTestCase):
    def setUp(self):
        self.device = "/dev/loop0"
        self.mount = "/tmp/moocbt"
        self.cow_file = "cow.snap"
        self.cow_full_path = "{}/{}".format(self.mount, self.cow_file)
        self.minor = 1
        self.snap_device = "/dev/moocbt{}".format(self.minor)

    def _ensure_mounted(self):
        if not os.path.ismount(self.mount):
            util.mount(self.device, self.mount)

    def test_umount_remount_snapshot(self):
        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)
        self.addCleanup(self._ensure_mounted)
        self.assertEqual(moocbt.info(self.minor)["state"], ACTIVE_SNAPSHOT)

        # Unmounting fires the umount hook -> dormant.
        util.unmount(self.mount)
        util.settle()
        self.assertEqual(moocbt.info(self.minor)["state"], DORMANT_SNAPSHOT)

        # Remounting fires the mount hook -> active.
        util.mount(self.device, self.mount)
        util.settle()
        info = moocbt.info(self.minor)
        self.assertEqual(info["state"], ACTIVE_SNAPSHOT)
        self.assertEqual(info["error"], 0)

    def test_umount_remount_incremental(self):
        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)
        self.addCleanup(self._ensure_mounted)
        self.assertEqual(moocbt.transition_to_incremental(self.minor), 0)
        self.assertEqual(moocbt.info(self.minor)["state"], ACTIVE_INCREMENTAL)

        util.unmount(self.mount)
        util.settle()
        self.assertEqual(moocbt.info(self.minor)["state"], DORMANT_INCREMENTAL)

        util.mount(self.device, self.mount)
        util.settle()
        info = moocbt.info(self.minor)
        self.assertEqual(info["state"], ACTIVE_INCREMENTAL)
        self.assertEqual(info["error"], 0)

    def test_fd_based_mount_activates(self):
        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)
        self.addCleanup(self._ensure_mounted)

        util.unmount(self.mount)
        util.settle()
        self.assertEqual(moocbt.info(self.minor)["state"], DORMANT_SNAPSHOT)

        try:
            util.mount_fd(self.device, self.mount, "ext4")
        except OSError as e:
            if e.errno == errno.ENOSYS:
                self.skipTest("kernel lacks the new mount API (move_mount)")
            raise
        util.settle()

        info = moocbt.info(self.minor)
        self.assertEqual(info["state"], ACTIVE_SNAPSHOT)
        self.assertEqual(info["error"], 0)

    def _write(self, name, content):
        path = "{}/{}".format(self.mount, name)
        with open(path, "w") as f:
            f.write(content)
        self.addCleanup(self._safe_remove, path)
        os.sync()


    def _safe_remove(path):
        try:
            os.remove(path)
        except FileNotFoundError:
            pass


if __name__ == "__main__":
    unittest.main()

