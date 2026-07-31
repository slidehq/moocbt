// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2023 Datto Inc.
// Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.

#ifndef FTRACE_HOOKING_H_INCLUDE
#define FTRACE_HOOKING_H_INCLUDE

#include <linux/mount.h>
#include <linux/version.h>
#include <linux/ftrace.h>
#include "tracer.h"
#include "includes.h"
#include "logging.h"
#include "bdev_state_handler.h"
#include "preempt_rethook.h"


#ifdef HAVE_UAPI_MOUNT_H
#include <uapi/linux/mount.h>
#endif

struct ftrace_hook {
	const char *name;
	void *function;
	void *original;
	unsigned long op_flags;
	bool direct_hook_call;

	unsigned long address;
	struct ftrace_ops ops;
};

#define HOOK(_name, _function, _original, _op_flags, _direct_hook_call) \
	{ \
		.name = (_name), \
		.function = (_function), \
		.original = (_original), \
		.op_flags = (_op_flags), \
		.direct_hook_call = (_direct_hook_call), \
	}

#define USE_FENTRY_OFFSET 0

#ifdef HAVE_FTRACE_REGS_H
#include <linux/ftrace_regs.h>
#endif //HAVE_FTRACE_REGS_H

#ifndef ftrace_regs_get_return_value
#ifdef ftrace_regs_return_value
#define ftrace_regs_get_return_value(fregs) \
	ftrace_regs_return_value(fregs)
#else //ftrace_regs_get_return_value
#define ftrace_regs_get_return_value(fregs) \
	regs_return_value(ftrace_get_regs(fregs))
#endif //ftrace_regs_return_value
#endif //ftrace_regs_get_return_value

#ifndef ftrace_regs_get_argument
#define ftrace_regs_get_argument(fregs, n) \
	regs_get_kernel_argument(ftrace_get_regs(fregs), n)
#endif //ftrace_regs_get_argument

#ifndef HAVE_FTRACE_REGS
#define ftrace_regs pt_regs
static __always_inline struct pt_regs *ftrace_get_regs(struct ftrace_regs *fregs)
{
	return fregs;
}
#endif //HAVE_FTRACE_REGS

#ifdef HAVE_FTRACE_OPS_FL_RECURSION_SAFE
#define FTRACE_OPS_FL_RECURSION FTRACE_OPS_FL_RECURSION_SAFE
#endif //HAVE_FTRACE_OPS_FL_RECURSION_SAFE

#ifndef UMOUNT_NOFOLLOW
#define UMOUNT_NOFOLLOW 0
#endif

#define handle_bdev_mount_nowrite(dir_name, follow_flags, idx_out)             \
        handle_bdev_mount_event(dir_name, follow_flags, idx_out, 0)
#define handle_bdev_mounted_writable(dir_name, idx_out)                        \
        handle_bdev_mount_event(dir_name, 0, idx_out, 1)


#ifdef HAVE_SYS_OLDUMOUNT
static asmlinkage long (*orig_oldumount)(char __user *);
#endif


int register_ftrace_hooks(void);
int unregister_ftrace_hooks(void);


#endif //FTRACE_HOOKING_H_INCLUDE


