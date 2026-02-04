## NAME

moo - Control the Moo Continuous Block Tracking kernel module.

## SYNOPSIS

`moo <sub-command> [<args>]`

## DESCRIPTION

`moo` is the userspace tool used to manage the moocbt kernel module. It provides an interface to create, delete, reload, transition, and configure on-disk snapshots and certain parameters of the kernel module itself.

This manual page describes `moo` briefly. More detail is available in the Git repository located at https://github.com/slidehq/moocbt. 

## OPTIONS
    -c cache-size
         Specify how big the in-memory data cache can grow to (in MB). Defaults to 300 MB.

    -f fallocate
         Specify the maximum size of the COW file on disk.

## SUB-COMMANDS

### setup-snapshot

`moo setup-snapshot [-c <cache size>] [-f <fallocate>] <block device> <cow file path> <minor>`

Sets up a snapshot of `<block device>`, saving all COW data to `<cow file path>`. The snapshot device will be `/dev/moocbt<minor>`. The minor number will be used as a reference number for all other `moo` commands. `<cow file path>` must be a path on the `<block device>`.

### reload-snapshot

`moo reload-snapshot [-c <cache size>] <block device> <cow file> <minor>`

Reloads a snapshot. This command is meant to be run before the block device is mounted, after a reboot or after the driver is unloaded. It notifies the kernel driver to expect the block device specified to come back online. This command requires that the snapshot was cleanly unmounted in snapshot mode beforehand. If this is not the case, the snapshot will be put into the failure state once it attempts to come online. The minor number will be used as a reference number for all other `moo` commands.

### reload-incremental

`moo reload-incremental [-c <cache size>] <block device> <cow file> <minor>`

Reloads a block device that was in incremental mode. See `reload-snapshot` for restrictions.

### transition-to-incremental

`moo transition-to-incremental <minor>`

Transitions a snapshot COW file to incremental mode, which only tracks which blocks have changed since a snapshot started. This will remove the associated snapshot device.

### transition-to-snapshot

`moo transition-to-snapshot [-f <fallocate>] <cow file> <minor>`

Transitions a block device in incremental mode to snapshot mode. This call ensures no writes are missed between tearing down the incremental and setting up the new snapshot. The new snapshot data will be recorded in `<cow file>`. The old cow file will still exist after this and can be used to efficiently copy only changed blocks using a tool succh as `update-img`.

### destroy

`moo destroy <minor>`

Cleanly and completely removes the snapshot or incremental, unlinking the associated COW file.

### reconfigure

`moo reconfigure [-c <cache size>] <minor>`

Allows you to reconfigure various parameters of a snapshot while it is online. Currently only the index cache size (given in MB) can be changed dynamically.

### expand-cow-file

`moo expand-cow-file <size> <minor>`

Expands cow file in snapshot mode by size (given in megabytes).

### reconfigure-auto-expand

`moo reconfigure-auto-expand [-n <steps limit>] <step size> <minor>`

Enable auto-expand of cow file in snapshot mode by <step size> (given in megabytes). Auto-expand works in that way that at least <reserved space> (given in megabytes) is left available after each step for regular users of filesystem.

### EXAMPLES

`# moo setup-snapshot /dev/sda1 /var/backup/moocow 4`

This command will set up a new COW snapshot device tracking `/dev/sda1` at `/dev/moocbt4`. This block device is backed by a new file created at the path `/var/backup/moocow`.

`# moo transition-to-incremental 4`

Transitions the snapshot specified by the minor number to incremental mode.

`# moo transition-to-snapshot /var/backup/moocow1 4`

Cleanly transitions the incremental to a new snapshot, using `/var/backup/moocow1` as the new COW file. At this point a second backup can be taken, either doing a full copy with a tool like `dd` or an incremental copy using a tool such as `update-img`, if a previous snapshot backup exists.

`# moo reconfigure -c 400 4`

Reconfigures the block device to have an in-memory index cache size of 400 MB.

`# moo destroy 4`

This will stop tracking `/dev/sda1`, remove the associated `/dev/moocbt4` (since the device is in snapshot mode), delete the COW file backing it, and perform all other cleanup.

`# moo reload-snapshot /dev/sda1 /var/backup/moocow1 4`

After a reboot, this command may be performed in the early stages of boot, before the block device is mounted read-write. This will notify the driver to expect a block device `/dev/sda1` that was left in snapshot mode to come online with a COW file located at `/var/backup/moocow1` (relative to the mountpoint), and that the reloaded snapshot should come online at minor number 4. If a problem is discovered when the block device comes online, this block device will be put into the failure state, which will be reported in `/proc/moocbt-info`

`# moo reload-incremental /dev/sda5 /var/backup/moocow1 4`

This will act the same as `reload-snapshot`, but for a device that was left in incremental mode.

## BUGS

## AUTHORS

Tom Caputi (tcaputi@datto.com)

Slide (it@slide.tech)

## COPYRIGHT

Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc
