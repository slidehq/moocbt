#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only

#
# Copyright (C) 2026 Project Orca Inc.
#

# End-to-end data-correctness tests for the snapshot / incremental backup cycle.

import os
import unittest

import moocbt
import util
from devicetestcase import DeviceTestCase


class TestBackupCycle(DeviceTestCase):
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

    def _write_file(self, name, content):
        path = "{}/{}".format(self.mount, name)
        with open(path, "w") as f:
            f.write(content)
        self.addCleanup(_safe_remove, path)
        os.sync()

    def _mount_snapshot_ro(self):
        util.mount(self.snap_device, self.snap_mount, opts="ro")
        self.addCleanup(util.unmount, self.snap_mount)

    def _snap_file_md5(self, name):
        return util.md5sum("{}/{}".format(self.snap_mount, name))

    def test_snapshot_frozen(self):
        # Mutations to the FS after setup must still read as its pre-snapshot
        # content through the snapshot.
        # This is verified at the filesystem level. The cow file lives on the
        # tracked volume and its blocks are excluded from COW operations, so
        # a full device hash is not suitable.
        self._write_file("base", "content")
        base_md5 = util.md5sum("{}/base".format(self.mount))

        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)

        # Mutate the live FS
        self._write_file("mutation", "additional content")
        self._write_file("base", "overwritten content")

        self._mount_snapshot_ro()
        # Overwritten file still reads as its pre-snapshot content.
        self.assertEqual(self._snap_file_md5("base"), base_md5)
        # A file created after the snapshot is absent from the frozen image.
        self.assertFalse(os.path.exists("{}/mutation".format(self.snap_mount)))

    def test_incremental_cycle_reflects_new_origin(self):
        # Test snapshot -> incremental -> write new data -> snapshot.
        # The second snapshot must capture the data written while incremental
        # and still serve the untouched baseline.
        self._write_file("base", "shared baseline that is never rewritten")
        base_md5 = util.md5sum("{}/base".format(self.mount))

        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)

        # Incremental
        self.assertEqual(moocbt.transition_to_incremental(self.minor), 0)
        delta = "D" * 200000
        self._write_file("delta", delta)

        # Snapshot
        self.assertEqual(
            moocbt.transition_to_snapshot(self.minor, self.cow_full_path_2), 0
        )
        self.assertEqual(moocbt.info(self.minor)["state"], 3)  # SNAPSHOT | ACTIVE

        self._mount_snapshot_ro()
        with open("{}/delta".format(self.snap_mount)) as f:
            self.assertEqual(f.read(), delta)
        self.assertEqual(self._snap_file_md5("base"), base_md5)

    def test_multi_round_incremental_stays_correct(self):
        # Each round writes a uniquely-named file and every prior file must
        # remain readable and correct through the newest snapshot.
        self.assertEqual(moocbt.setup(self.minor, self.device, self.cow_full_path), 0)
        self.addCleanup(moocbt.destroy, self.minor)

        expected = {}
        for i in range(3):
            self.assertEqual(moocbt.transition_to_incremental(self.minor), 0)
            name = "round{}".format(i)
            content = "round {} payload {}".format(i, "y" * 40000)
            self._write_file(name, content)
            expected[name] = content

            cow = "{}/round{}.snap".format(self.mount, i)
            self.assertEqual(moocbt.transition_to_snapshot(self.minor, cow), 0)
            self.assertEqual(moocbt.info(self.minor)["error"], 0)

            # Must unmount before the next transition_to_incremental, so mount
            # directly here rather than via addCleanup.
            util.mount(self.snap_device, self.snap_mount, opts="ro")
            try:
                for fname, fcontent in expected.items():
                    with open("{}/{}".format(self.snap_mount, fname)) as f:
                        self.assertEqual(f.read(), fcontent,
                                         "{} wrong after round {}".format(fname, i))
            finally:
                util.unmount(self.snap_mount)


def _safe_remove(path):
    try:
        os.remove(path)
    except FileNotFoundError:
        pass


if __name__ == "__main__":
    unittest.main()
