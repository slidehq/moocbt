// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2026 Project Orca Inc.
 */

#include "includes.h"

MODULE_LICENSE("GPL");

static inline int dummy(void){
	return __NR_move_mount;
}
