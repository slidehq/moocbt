#!/bin/bash
# we need the moocbt module and the moo binary in the initramfs
# blkid is already there but I just want to be extra sure. shmura.

#%stage: volumemanager
#%depends: lvm2
#%modules: moocbt
#%programs: /usr/bin/moo
#%programs: /sbin/lsmod
#%programs: /sbin/modprobe
#%programs: /sbin/blkid
#%programs: /usr/sbin/blkid
#%programs: /sbin/blockdev
#%programs: /usr/sbin/blockdev
#%programs: /bin/mount
#%programs: /usr/bin/mount
#%programs: /bin/umount
#%programs: /usr/bin/umount
#%programs: /bin/udevadm
#%programs: /usr/bin/udevadm

echo "moocbt load_modules" > /dev/kmsg
# this is a function in linuxrc, modprobes moocbt for us.
load_modules

/sbin/modprobe --allow-unsupported moocbt

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
    [ -z "$rootfstype" ] && rootfstype=$(blkid -s TYPE -o value $rbd)

    echo "moocbt: mounting $rbd as $rootfstype" > /dev/kmsg
    blockdev --setro $rbd
    mount -t $fstype -o ro "$rbd" /etc/moocbt/mnt > /dev/kmsg
    udevadm settle

    if [ -x /sbin/moo_reload ]; then
        /sbin/moo_reload
    else
        echo "moocbt: error: cannot reload tracking data: missing /sbin/moo_reload" > /dev/kmsg
    fi

    umount -f /etc/moocbt/mnt > /dev/kmsg
    blockdev --setrw $rbd
fi
