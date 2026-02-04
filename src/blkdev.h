// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2022 Datto Inc.
 * Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
 */

#ifndef BLKDEV_H_
#define BLKDEV_H_

#include "config.h"
#include "includes.h"

struct block_device;

#ifndef bdev_whole
//#if LINUX_VERSION_CODE < KERNEL_VERSION(5,11,0)
#define bdev_whole(bdev) ((bdev)->bd_contains)
#endif

struct bdev_wrapper{
    struct block_device* bdev;

    union {
#ifdef HAVE_BDEV_HANDLE
        struct bdev_handle* handle;
#endif
// Kernel 6.9+ manages block_device with struct file and file_bdev has to be used to find block_device from file.
// For us, file_bdev function is marker to check if we have to use 
#ifdef USE_BDEV_AS_FILE
        struct file* file;
#endif
    } _internal;
};

#ifndef HAVE_HD_STRUCT
//#if LINUX_VERSION_CODE < KERNEL_VERSION(5,11,0)
#define moocbt_bdev_size(bdev) (bdev_nr_sectors(bdev))
#elif !defined HAVE_PART_NR_SECTS_READ
//#if LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
#define moocbt_bdev_size(bdev) ((bdev)->bd_part->nr_sects)
#else
#define moocbt_bdev_size(bdev) part_nr_sects_read((bdev)->bd_part)
#endif


struct bdev_wrapper *moocbt_blkdev_by_path(const char *path, fmode_t mode,
                                        void *holder);

struct super_block *moocbt_get_super(struct block_device * bd);

void moocbt_drop_super(struct super_block *sb);

void moocbt_blkdev_put(struct bdev_wrapper *bd);

int moocbt_get_start_sect_by_gendisk_for_bio(struct gendisk* gd, u8 partno, sector_t* result);

int moocbt_get_kstatfs(struct block_device* bd, struct kstatfs* statfs);
#endif /* BLKDEV_H_ */
