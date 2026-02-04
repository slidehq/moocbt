// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2015 Datto Inc.
 * Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
 */

#include "includes.h"

MODULE_LICENSE("GPL");

static inline void dummy(void){
    struct block_device bd;
    struct super_block sb;

    if(thaw_bdev(&bd, &sb)) bd.bd_disk = 0;
}
