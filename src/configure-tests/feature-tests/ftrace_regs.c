// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2015 Datto Inc.
 * Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
 */

#include "includes.h"
#include <linux/ftrace.h>

MODULE_LICENSE("GPL");

static inline void dummy(void){
	struct ftrace_regs fregs;
	struct pt_regs *regs = ftrace_get_regs(&fregs);
	(void) regs;
}
