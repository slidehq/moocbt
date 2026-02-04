// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2025 Datto Inc.
 * Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
 */

#include "includes.h"

MODULE_LICENSE("GPL");

static inline void dummy(void){
    struct block_device *bdev = NULL;
    unsigned flag;

    bdev_set_flag(bdev, flag);
}
 