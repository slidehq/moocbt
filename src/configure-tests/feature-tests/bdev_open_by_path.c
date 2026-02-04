// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2024 Datto Inc.
 * Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
 */

#include "includes.h"

MODULE_LICENSE("GPL");

static inline void dummy(void){
	struct bdev_handle* bh;
    
    const char *path;
    blk_mode_t mode;
    int holder;
    const struct blk_holder_ops bho;

    bh = bdev_open_by_path(path, mode, (void*)&holder, &bho);
}
