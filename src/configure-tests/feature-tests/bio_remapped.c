// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2023 Datto Inc.
 * Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
 */

#include "includes.h"

MODULE_LICENSE("GPL");

static inline void dummy(void){
    struct bio *new_bio = NULL;
    bio_set_flag(new_bio, BIO_REMAPPED);
}