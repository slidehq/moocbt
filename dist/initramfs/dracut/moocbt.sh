#!/bin/sh

type getarg >/dev/null 2>&1 || . /lib/dracut-lib.sh

modprobe moocbt

[ -z "$root" ] && root=$(getarg root=)
[ -z "$rootfstype" ] && rootfstype=$(getarg rootfstype=)

rbd="${root#block:}"
if [ -n "$rbd" ]; then
    case "$rbd" in
        LABEL=*)
            rbd="$(echo $rbd | sed 's,/,\\x2f,g')"
            rbd="/dev/disk/by-label/${rbd#LABEL=}"
            ;;
        UUID=*)
            rbd="/dev/disk/by-uuid/${rbd#UUID=}"
            ;;
        PARTLABEL=*)
            rbd="/dev/disk/by-partlabel/${rbd#PARTLABEL=}"
            ;;
        PARTUUID=*)
            rbd="/dev/disk/by-partuuid/${rbd#PARTUUID=}"
            ;;
    esac

    echo "moocbt: root block device = $rbd" > /dev/kmsg

    # Device might not be ready
    if [ ! -b "$rbd" ]; then
        udevadm settle
    fi

    # Kernel cmdline might not specify rootfstype
    [ -z "$rootfstype" ] && rootfstype=$(blkid -s TYPE "$rbd" -o value)

    echo "moocbt: mounting $rbd as $rootfstype" > /dev/kmsg
    blockdev --setro $rbd
    mount -t $rootfstype -o ro "$rbd" /etc/moocbt/mnt
    udevadm settle

    if [ -x /sbin/moo_reload ]; then
        /sbin/moo_reload
    else
        echo "moocbt: error: cannot reload tracking data: missing /sbin/moo_reload" > /dev/kmsg
    fi

    umount -f /etc/moocbt/mnt
    blockdev --setrw $rbd
fi
