#!/bin/bash
#%stage: block

# we need a directory to mount the root filesystem in
# when the boot-moocbt.sh script runs so it can run reload

export MOUNTROOT=$tmp_mnt/etc/moocbt/mnt
echo "moocbt install making mountpoint directory $MOUNTROOT" > /dev/kmsg
mkdir -p $MOUNTROOT
cp /var/lib/moocbt/reload $tmp_mnt/sbin/moo_reload

mkdir -p $tmp_mnt/usr/bin
mkdir -p $tmp_mnt/usr/sbin
