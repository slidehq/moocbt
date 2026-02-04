// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2025 Datto Inc.
 * Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
 */

#include "includes.h"

MODULE_LICENSE("GPL");

static inline void dummy(void){
    char *dest;
    const char *src;
    size_t count;
    size_t ret;
    
    (void)ret;

    ret = strlcpy(dest, src, count);
}