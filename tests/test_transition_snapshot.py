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


class TestTransitionToSnapshot(DeviceTestCase):
    def setUp(self):
        self.device = "/dev/loop0"
        self.mount = "/tmp/moocbt"
        self.cow_file = "cow.snap"
        self.cow_full_path = "{}/{}".format(self.mount, self.cow_file)
        self.cow_file_2 = "cow2.snap"
        self.cow_full_path_2 = "{}/{}".format(self.mount, self.cow_file_2)
        self.minor = 1

        self.snap_mount = "/mnt"
        self.snap_device = "/dev/moocbt{}".format(self.minor)

    def test_transition_nonexistent_device(self):
        self.assertIsNone(moocbt.info(self.minor))
        self.assertEqual(
            moocbt.transition_to_snapshot(self.minor, self.cow_full_path),
            errno.ENOENT,
        )

    def test_transition_active_snapshot(self):
        # Transitioning a device in active snapshot mode to snapshot is invalid.
        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)

        self.assertEqual(
            moocbt.transition_to_snapshot(self.minor, self.cow_full_path),
            errno.EINVAL,
        )

    def test_transition_active_incremental(self):
        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)

        self.assertEqual(moocbt.transition_to_incremental(self.minor), 0)

        self.assertEqual(
            moocbt.transition_to_snapshot(self.minor, self.cow_full_path_2), 0
        )

        # Verify the snapshot block device is recreated and the device is back in
        # active snapshot mode (SNAPSHOT | ACTIVE).
        self.assertTrue(os.path.exists(self.snap_device))
        snapdev = moocbt.info(self.minor)
        self.assertIsNotNone(snapdev)
        self.assertEqual(snapdev["error"], 0)
        self.assertEqual(snapdev["state"], 3)

    def test_transition_lifecycle(self):
        # snapshot -> incremental -> snapshot -> incremental should alternate
        # cleanly between active states.
        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)

        self.assertEqual(moocbt.transition_to_incremental(self.minor), 0)
        self.assertEqual(moocbt.info(self.minor)["state"], 2) # ACTIVE

        self.assertEqual(
            moocbt.transition_to_snapshot(self.minor, self.cow_full_path_2), 0
        )
        self.assertEqual(moocbt.info(self.minor)["state"], 3) # SNAPSHOT | ACTIVE

        self.assertEqual(moocbt.transition_to_incremental(self.minor), 0)
        self.assertEqual(moocbt.info(self.minor)["state"], 2) # ACTIVE

    def test_transition_snapshot_tracks_writes(self):
        # After transitioning back to snapshot mode, the new snapshot must
        # preserve overwritten data in the cow file.
        testfile = "{}/testfile".format(self.mount)
        snapfile = "{}/testfile".format(self.snap_mount)

        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)
        self.assertEqual(moocbt.transition_to_incremental(self.minor), 0)

        with open(testfile, "w") as f:
            f.write("The quick brown fox")

        self.addCleanup(os.remove, testfile)
        os.sync()
        md5_orig = util.md5sum(testfile)

        self.assertEqual(moocbt.transition_to_snapshot(self.minor, self.cow_full_path_2), 0)

        with open(testfile, "w") as f:
            f.write("jumps over the lazy dog")

        os.sync()

        util.mount(self.snap_device, self.snap_mount, opts="ro")
        self.addCleanup(util.unmount, self.snap_mount)

        md5_snap = util.md5sum(snapfile)
        self.assertEqual(md5_orig, md5_snap)


if __name__ == "__main__":
    unittest.main()
