// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2026 Project Orca Inc.
 */

#include "includes.h"
#include <linux/ftrace.h>

MODULE_LICENSE("GPL");

static inline void dummy(void) {
	unsigned long flag = FTRACE_OPS_FL_RECURSION_SAFE;
}

