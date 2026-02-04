// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2017 Datto Inc.
 * Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
 */

#include "includes.h"
#include <linux/uuid.h>

MODULE_LICENSE("GPL");

static inline void dummy(void){
	generate_random_uuid(NULL);
}
