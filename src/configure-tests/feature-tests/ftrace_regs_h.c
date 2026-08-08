// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2026 Project Orca Inc.
 */

#include "includes.h"
#include <linux/ftrace.h>
#include <linux/ftrace_regs.h>

MODULE_LICENSE("GPL");

static inline void dummy(void) {
        struct ftrace_regs *fregs;
        unsigned long ret = ftrace_regs_get_return_value(fregs);
        (void) ret;
}

