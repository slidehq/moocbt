#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only

#
# Copyright (C) 2019 Datto, Inc.
# Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
#

import errno
import os
import unittest

import moocbt
import util
from devicetestcase import DeviceTestCase


class TestDestroy(DeviceTestCase):
    def setUp(self):
        self.device = "/dev/loop0"
        self.mount = "/tmp/moocbt"
        self.cow_file = "cow.snap"
        self.cow_full_path = "{}/{}".format(self.mount, self.cow_file)
        self.cow_reload_path = "/{}".format(self.cow_file)
        self.minor = 1
        self.snap_device = "/dev/moocbt{}".format(self.minor)

    def test_destroy_nonexistent_device(self):
        self.assertEqual(moocbt.destroy(self.minor), errno.ENOENT)

    def test_destroy_active_snapshot(self):
        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)

        self.assertEqual(moocbt.destroy(self.minor), 0)
        self.assertFalse(os.path.exists(self.snap_device))
        self.assertIsNone(moocbt.info(self.minor))

    def test_destroy_active_incremental(self):
        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)
        self.assertEqual(moocbt.transition_to_incremental(self.minor), 0)

        self.assertEqual(moocbt.destroy(self.minor), 0)
        self.assertFalse(os.path.exists(self.snap_device))
        self.assertIsNone(moocbt.info(self.minor))

    @unittest.skip("Broken since 4.17 (see #144)")
    def test_destroy_dormant_snapshot(self):
        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)

        util.unmount(self.mount)
        self.addCleanup(os.remove, self.cow_full_path)
        self.addCleanup(util.mount, self.device, self.mount)

        self.assertEqual(moocbt.destroy(self.minor), 0)
        self.assertFalse(os.path.exists(self.snap_device))
        self.assertIsNone(moocbt.info(self.minor))

    @unittest.skip("Broken since 4.17 (see #144)")
    def test_destroy_dormant_incremental(self):
        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)
        self.assertEqual(moocbt.transition_to_incremental(self.minor), 0)

        util.unmount(self.mount)
        self.addCleanup(os.remove, self.cow_full_path)
        self.addCleanup(util.mount, self.device, self.mount)

        self.assertEqual(moocbt.destroy(self.minor), 0)
        self.assertFalse(os.path.exists(self.snap_device))
        self.assertIsNone(moocbt.info(self.minor))

    def test_destroy_unverified_snapshot(self):
        util.unmount(self.mount)
        self.addCleanup(util.mount, self.device, self.mount)
        self.assertEqual(moocbt.reload_snapshot(self.minor, self.device, self.cow_reload_path), 0)

        self.assertEqual(moocbt.destroy(self.minor), 0)
        self.assertFalse(os.path.exists(self.snap_device))
        self.assertIsNone(moocbt.info(self.minor))

    def test_destroy_unverified_incremental(self):
        util.unmount(self.mount)
        self.addCleanup(util.mount, self.device, self.mount)
        self.assertEqual(moocbt.reload_incremental(self.minor, self.device, self.cow_reload_path), 0)

        self.assertEqual(moocbt.destroy(self.minor), 0)
        self.assertFalse(os.path.exists(self.snap_device))
        self.assertIsNone(moocbt.info(self.minor))


if __name__ == "__main__":
    unittest.main()
