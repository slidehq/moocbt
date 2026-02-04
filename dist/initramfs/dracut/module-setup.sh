#!/bin/sh
# -*- mode: shell-script; indent-tabs-mode: nil; sh-basic-offset: 4; -*-
# ex: ts=8 sw=4 sts=4 et filetype=sh

check() {
    return 0
}

depends() {
    return 0
}

installkernel() {
    hostonly='' instmods moocbt
}

install() {
    inst_hook pre-mount 01 "$moddir/moocbt.sh"
    inst_dir /etc/moocbt/mnt
    inst /sbin/blkid
    inst /sbin/blockdev
    inst /usr/bin/udevadm
    inst /usr/bin/moo
    inst_simple /var/lib/moocbt/reload /sbin/moo_reload
}
