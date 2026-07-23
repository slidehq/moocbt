// SPDX-License-Identifier: GPL-2.0-only

/* Copyright (C) 2026 Project Orca Inc.
 */

#include "includes.h"
#include <linux/blk_types.h>
#include <linux/bio.h>

MODULE_LICENSE("GPL");

static inline void dummy(void){
	struct bio* src = NULL;
	struct bio* dst = NULL;
	bio_clone_blkg_association(dst, src);
}

