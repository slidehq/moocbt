## NAME

update-img - Update a backup image with moocbt COW file.

## SYNOPSIS

`update-img <snapshot device> <cow file> <image file>`

## DESCRIPTION

`update-img` is a simple tool to efficiently update backup images made by the moocbt kernel module. It uses the leftover COW file from moocbt's incremental state to efficiently update an existing backup image. See the man page on `moo` for an example use case.

### EXAMPLES

`# update-img /dev/moocbt4 /var/backup/moocow1 /mnt/data/backup-img`

This command will update a previously backed up snapshot `/mnt/data/backup-img` with the changed blocks indicated by `/var/backup/moocow1` from `/dev/moocbt4`.

NOTE: `<snapshot device>` MUST be the NEXT snapshot after the one that `<image file>` was copied from.

## BUGS

## AUTHORS

Tom Caputi (tcaputi@datto.com)

Slide (it@slide.tech)

## COPYRIGHT

Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc
