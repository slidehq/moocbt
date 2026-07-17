#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only

#
# Copyright (C) 2019 Datto, Inc.
# Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
#

import ctypes
import ctypes.util
import hashlib
import os
import subprocess


def mount(device, path, opts=None):
    cmd = ["mount", device, path]
    if opts:
        cmd += ["-o", opts]

    subprocess.check_call(cmd, timeout=10)


def unmount(path):
    cmd = ["umount", path]
    subprocess.check_call(cmd, timeout=10)


def dd(ifile, ofile, count, **kwargs):
    cmd = ["dd", "status=none", "if={}".format(ifile), "of={}".format(ofile), "count={}".format(count)]
    for k, v in kwargs.items():
        cmd.append("{}={}".format(k, v))

    subprocess.check_call(cmd, timeout=20)


def md5sum(path):
    md5 = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            md5.update(chunk)

    return md5.hexdigest()


def settle(timeout=20):
    cmd = ["udevadm", "settle", "-t", "{}".format(timeout)]
    subprocess.check_call(cmd, timeout=(timeout + 10))


def loop_create(loop, path):
    cmd = ["losetup", loop, path]
    subprocess.check_call(cmd, timeout=10)


def loop_destroy(loop):
    cmd = ["losetup", "-d", loop]
    subprocess.check_call(cmd, timeout=10)


def mkfs(device):
    cmd = ["mkfs.ext4", "-F", device]
    subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)


_NR_open_tree = 428
_NR_move_mount = 429
_NR_fsopen = 430
_NR_fsconfig = 431
_NR_fsmount = 432

FSCONFIG_SET_STRING = 1
FSCONFIG_CMD_CREATE = 6
MOVE_MOUNT_F_EMPTY_PATH = 0x00000004
AT_FDCWD = -100

_libc = ctypes.CDLL(None, use_errno=True)


def _syscall(number, *args):
    _libc.syscall.restype = ctypes.c_long
    # Coerce plain ints to 64-bit so fds/flags/AT_FDCWD aren't truncated in the
    # argument registers; leave bytes (char*) and None (NULL) for ctypes.
    coerced = [ctypes.c_long(a) if isinstance(a, int) else a for a in args]
    res = _libc.syscall(ctypes.c_long(number), *coerced)
    if res < 0:
        err = ctypes.get_errno()
        raise OSError(err, os.strerror(err))
    return res


def mount_fd(device, path, fsname):
    # Mount `device` at `path` using move_mount() with a detached mount
    fsname = fsname.encode("utf-8")
    fs_fd = _syscall(_NR_fsopen, fsname, 0)
    try:
        _syscall(_NR_fsconfig, fs_fd, FSCONFIG_SET_STRING,
                 b"source", device.encode("utf-8"), 0)
        _syscall(_NR_fsconfig, fs_fd, FSCONFIG_CMD_CREATE, None, None, 0)
        mnt_fd = _syscall(_NR_fsmount, fs_fd, 0, 0)
    finally:
        os.close(fs_fd)
    try:
        _syscall(_NR_move_mount, mnt_fd, b"", AT_FDCWD,
                 path.encode("utf-8"), MOVE_MOUNT_F_EMPTY_PATH)
    finally:
        os.close(mnt_fd)

