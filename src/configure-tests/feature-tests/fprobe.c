// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 20206 Project Orca Inc.
 */

#include "includes.h"

MODULE_LICENSE("GPL");

static inline void dummy(void){
	struct fprobe fprobe;
	fprobe.entry_data_size = 1;
}
