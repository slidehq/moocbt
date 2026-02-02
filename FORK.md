# moocbt

This project is derived from DattoBD by Datto, available [here](github.com/datto/dattobd). This project maintains original GPL-2.0/LGPL-2.1 license terms at time of forking.

## Purpose

This fork aims to provide a block device snapshotting driver, providing point-in-time snapshots and change tracking for block devices.

## Changes

Notable changes from the original are listed below.

- Name changes
    - dattobd -> moocbt
    - libdattobd -> libmoocbt
    - dbdctl -> moo
    - cow file from .datto(1) -> .moocow(1)
    - /dev/dattoN -> /dev/moocbtN
    - /proc/datto-info -> /proc/moocbt-info
    - /proc/datto-ctl -> /proc/moocbt-ctl
    - Function symbols dattobd_* -> moocbt_*
