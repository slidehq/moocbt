// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2026 Project Orca Inc.
 */


#ifndef PREEMPT_RETHOOK_H_
#define PREEMPT_RETHOOK_H_

#include <linux/llist.h>
#include <linux/ptrace.h>
#include <linux/types.h>

typedef void (*preempt_rethook_handler_t)(void *, unsigned long, struct pt_regs *);

struct preempt_rethook {
	preempt_rethook_handler_t post_hook;
	size_t data_size;
	void *data;
};

struct preempt_rh_node {
	struct llist_node llist;
	preempt_rethook_handler_t post_hook;
	unsigned long ret_addr;
	unsigned long frame;
	void *data;
};

int pre_handler_preempt_rethook(struct preempt_rethook *prh,
		struct pt_regs *regs);

void preempt_rethook_hook(struct preempt_rh_node *node, struct pt_regs *regs);

struct preempt_rh_node *preempt_rethook_try_get(struct preempt_rethook *prh);

unsigned long preempt_rethook_trampoline_handler(struct pt_regs *regs,
		unsigned long frame);

#endif //PREEMPT_RETHOOK_H_

