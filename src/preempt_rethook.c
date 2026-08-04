// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2026 Project Orca Inc.
 */

#include "preempt_rethook.h"
#include "logging.h"

#include <linux/kprobes.h>
#include <linux/objtool.h>
#include <linux/kernel.h>
#include <linux/preempt.h>
#include <linux/hashtable.h>
#include <linux/llist.h>
#include <asm/frame.h>
#include <asm/ptrace.h>
#include <asm/segment.h>
#include <asm/unwind_hints.h>

// arch/x86/kernel/kprobes/common.h

#ifdef CONFIG_X86_64

#define SAVE_REGS_STRING \
	/* Skip cs, ip, orig_ax. */ \
	"    subq $24, %rsp\n" \
	"    pushq %rdi\n" \
	"    pushq %rsi\n" \
	"    pushq %rdx\n" \
	"    pushq %rcx\n" \
	"    pushq %rax\n" \
	"    pushq %r8\n" \
	"    pushq %r9\n" \
	"    pushq %r10\n" \
	"    pushq %r11\n" \
	"    pushq %rbx\n" \
	"    pushq %rbp\n" \
	"    pushq %r12\n" \
	"    pushq %r13\n" \
	"    pushq %r14\n" \
	"    pushq %r15\n" \
	ENCODE_FRAME_POINTER

#define RESTORE_REGS_STRING \
	"    popq %r15\n" \
	"    popq %r14\n" \
	"    popq %r13\n" \
	"    popq %r12\n" \
	"    popq %rbp\n" \
	"    popq %rbx\n" \
	"    popq %r11\n" \
	"    popq %r10\n" \
	"    popq %r9\n" \
	"    popq %r8\n" \
	"    popq %rax\n" \
	"    popq %rcx\n" \
	"    popq %rdx\n" \
	"    popq %rsi\n" \
	"    popq %rdi\n" \
	/* Skip orig_ax, ip, cs */ \
	"    addq $24, %rsp\n"

#else //CONFIG_X86_64

#define SAVE_REGS_STRING \
	/* Skip cs, ip, orig_ax and gs. */ \
	"    subl $4*4, %esp\n" \
	"    pushl %fs\n" \
	"    pushl %es\n" \
	"    pushl %ds\n" \
	"    pushl %eax\n" \
	"    pushl %ebp\n" \
	"    pushl %edi\n" \
	"    pushl %esi\n" \
	"    pushl %edx\n" \
	"    pushl %ecx\n" \
	"    pushl %ebx\n" \
	ENCODE_FRAME_POINTER

#define RESTORE_REGS_STRING \
	"    popl %ebx\n" \
	"    popl %ecx\n" \
	"    popl %edx\n" \
	"    popl %esi\n" \
	"    popl %edi\n" \
	"    popl %ebp\n" \
	"    popl %eax\n" \
	/* Skip ds, es, fs, gs, orig_ax, ip and cs. */ \
	"    addl $7*4, %esp\n"

#endif //CONFIG_X86_64

// arch/x86/kernel/rethook.c

__visible void preempt_rethook_trampoline_callback(struct pt_regs *regs);

#ifndef ANNOTATE_NOENDBR
#define ANNOTATE_NOENDBR
#endif

void preempt_rethook_trampoline(void);

/*
 * When a target function returns, this code saves registers and calls
 * preempt_rethook_trampoline_callback(), which calls the preempt_rethook
 * handler.
 */
asm(
	".text\n"
	".global preempt_rethook_trampoline\n"
	".type preempt_rethook_trampoline, @function\n"
	"preempt_rethook_trampoline:\n"
#ifdef CONFIG_X86_64
	ANNOTATE_NOENDBR "\n" /* This is only jumped from ret instruction */
	/* Push a fake return address to tell the unwinder it's a rethook. */
	"    pushq $preempt_rethook_trampoline\n"
	UNWIND_HINT_FUNC
	"    pushq $" __stringify(__KERNEL_DS) "\n"
	/* Save the 'sp - 16', this will be fixed later. */
	"    pushq %rsp\n"
	"    pushfq\n"
	SAVE_REGS_STRING
	"    movq %rsp, %rdi\n"
	"    call preempt_rethook_trampoline_callback\n"
	RESTORE_REGS_STRING
	/* In the callback function, 'regs->flags' is copied to 'regs->ss'. */
	"    addq $16, %rsp\n"
	"    popfq\n"
#else //CONFIG_X86_64
	/* Push a fake return address to tell the unwinder it's a rethook. */
	"    pushl $preempt_rethook_trampoline\n"
	UNWIND_HINT_FUNC
	"    pushl %ss\n"
	/* Save the 'sp - 8', this will be fixed later. */
	"    pushl %esp\n"
	"    pushfl\n"
	SAVE_REGS_STRING
	"    movl %esp, %eax\n"
	"    call preempt_rethook_trampoline_callback\n"
	RESTORE_REGS_STRING
	/* In the callback function, 'regs->flags' is copied to 'regs->ss'. */
	"    addl $8, %esp\n"
	"    popfl\n"
#endif //CONFIG_X86_64
       ASM_RET
       ".size preempt_rethook_trampoline, .-preempt_rethook_trampoline\n"
);

/*
 * Called from preempt_rethook_trampoline
 */
__used __visible void preempt_rethook_trampoline_callback(
		struct pt_regs *regs) {
	unsigned long *frame_pointer;

	/* fixup registers */
	regs->cs = __KERNEL_CS;
#ifdef CONFIG_X86_32
	regs->gs = 0;
#endif //CONFIG_X86_32
	regs->ip = (unsigned long)&preempt_rethook_trampoline;
	regs->orig_ax = ~0UL;
	regs->sp += 2 * sizeof(long);
	frame_pointer = (long *)(regs + 1);

	/*
	 * The return address at 'frame_pointer' is recovered by the
	 * arch_preempt_rethook_fixup_return() which is called from this
	 * preempt_rethook_trampoline_handler().
	 */
	preempt_rethook_trampoline_handler(regs, (unsigned long)frame_pointer);

	/*
	 * Copy FLAGS to 'pt_regs::ss' so that preempt_rethook_trampoline()
	 * can do RET right after POPF.
	 */
	*(unsigned long *)&regs->ss = regs->flags;
}

/* This is called from preempt_rethook_trampoline_handler(). */
static void preempt_rethook_fixup_return(struct pt_regs *regs,
		unsigned long correct_ret_addr) {
	unsigned long *frame_pointer = (void *)(regs + 1);

	/* Replace fake return address with real one. */
	*frame_pointer = correct_ret_addr;
}

static void preempt_rethook_prepare(struct preempt_rh_node *prhn,
		struct pt_regs *regs) {
	unsigned long *stack = (unsigned long *)regs->sp;
	prhn->ret_addr = stack[0];
	prhn->frame = regs->sp;

	/* Replace the return addr with trampoline addr */
	stack[0] = (unsigned long) preempt_rethook_trampoline;
}

// kernel/trace/rethook.c

/*
 * This pre handler is called by every preemptable rethook.
 * When the hook triggers, it will set up the post-hook.
 */
int pre_handler_preempt_rethook(struct preempt_rethook *prh,
		struct pt_regs *regs) {
	struct preempt_rh_node *prhn = preempt_rethook_try_get(prh);
	if (!prhn) {
		return -ENOMEM;
	}
	preempt_rethook_hook(prhn, regs);
	return 0;
}

struct preempt_rh_task_node {
	struct hlist_node node;
	struct task_struct *tsk;
	struct llist_head llist;
};

static DEFINE_HASHTABLE(preempt_rh_task_nodes, 6);
static DEFINE_SPINLOCK(preempt_rh_task_nodes_lock);

static struct preempt_rh_task_node *preempt_rh_task_node_find(
		struct task_struct *tsk) {
	struct preempt_rh_task_node *prhtn;
	hash_for_each_possible(preempt_rh_task_nodes, prhtn, node, (unsigned long) tsk) {
		if (prhtn->tsk == tsk) {
			return prhtn;
		}
	}
	return NULL;
}

/**
 * preempt_rethook_recycle() - return the node to preempt_rethook.
 * @node: The struct preempt_rh_node to be returned.
 *
 * Return back the @node to @node::preempt_rethook. If the
 * @node::prempt_rethook is already marked as freed, this will free the @node.
 */
static void preempt_rethook_recycle(struct preempt_rh_node *node) {
	if (node->data) {
		kfree(node->data);
	}
	kfree(node);
}

/**
 * preempt_rethook_try_get() - get an unused preempt rethook node.
 * @prh: The struct preempt_rethook which pools the nodes.
 *
 * Get an unused preempt rethook node from @prh. If the node is empty, this
 * will return NULL. Caller must disable preemption.
 */
struct preempt_rh_node *preempt_rethook_try_get(struct preempt_rethook *prh) {
	struct preempt_rh_node *node = kmalloc(sizeof(struct preempt_rh_node), GFP_ATOMIC);
	if (!node) {
		return NULL;
	}
	if (prh->data_size) {
		node->data = kmalloc(prh->data_size, GFP_ATOMIC);
		if (!node->data) {
			kfree(node);
			return NULL;
		}
		memcpy(node->data, prh->data, prh->data_size);
	} else {
		node->data = NULL;
	}
	node->post_hook = prh->post_hook;
	return node;
}

/**
 * preempt_rethook_hook() - Hook the current function return.
 * @node: The struct preempt_rh_node to hook the function return.
 * @regs: The struct pt_regs for the function entry.
 *
 * Hook the current running function return. This must be called when the
 * function entry (or at least @regs must be the registers of the function
 * entry.)
 */
void preempt_rethook_hook(struct preempt_rh_node *node, struct pt_regs *regs) {
	spin_lock(&preempt_rh_task_nodes_lock);
	struct preempt_rh_task_node *prhtn = preempt_rh_task_node_find(current);
	if (!prhtn) {
		prhtn = kmalloc(sizeof(struct preempt_rh_task_node), GFP_ATOMIC);
		if (!prhtn) {
			LOG_ERROR(ENOMEM, "failed to allocate preempt_rh_task_node");
			spin_unlock(&preempt_rh_task_nodes_lock);
			return;
		}
		init_llist_head(&prhtn->llist);
		prhtn->tsk = current;
		hash_add(preempt_rh_task_nodes, &prhtn->node, (unsigned long) prhtn->tsk);
	}
	spin_unlock(&preempt_rh_task_nodes_lock);

	preempt_rethook_prepare(node, regs);
	llist_add(&node->llist, &prhtn->llist);
}

/* This assumes the 'tsk' is the current task or is not running. */
static unsigned long preempt_rethook_find_ret_addr(struct task_struct *tsk,
		struct llist_node **cur) {
	struct preempt_rh_node *prhn = NULL;
	struct llist_node *node = *cur;

	if (!node) {
		spin_lock(&preempt_rh_task_nodes_lock);
		struct preempt_rh_task_node *prhtn = preempt_rh_task_node_find(tsk);
		spin_unlock(&preempt_rh_task_nodes_lock);
		if (prhtn) {
			node = prhtn->llist.first;
		} else {
			LOG_ERROR(1, "preempt_rethook: failed to find preempt_rh_task_node for ret addr");
			WARN_ON_ONCE(1);
		}
	} else {
		node = node->next;
	}

	while (node) {
		prhn = container_of(node, struct preempt_rh_node, llist);
		if (prhn->ret_addr != (unsigned long)preempt_rethook_trampoline) {
			*cur = node;
			return prhn->ret_addr;
		}
		node = node->next;
	}
	return 0;
}

unsigned long preempt_rethook_trampoline_handler(struct pt_regs *regs,
		unsigned long frame) {
	struct llist_node *first, *node = NULL;
	unsigned long correct_ret_addr;
	struct preempt_rh_node *prhn;

	correct_ret_addr = preempt_rethook_find_ret_addr(current, &node);
	if (!correct_ret_addr) {
		LOG_ERROR(1, "preempt_rethook: return address not found");
		BUG_ON(1);
	}

	instruction_pointer_set(regs, correct_ret_addr);

	/*
	 * Run the handler on the shadow stack. Do no unlink the list here
	 * because stackdump inside the handlers needs to decode it.
	 */
	spin_lock(&preempt_rh_task_nodes_lock);
	struct preempt_rh_task_node *prhtn = preempt_rh_task_node_find(
			current);
	spin_unlock(&preempt_rh_task_nodes_lock);
	if (!prhtn) {
		LOG_ERROR(1, "preempt_rethook: preempt_rh_task_node not found in trampoline handler");
		BUG_ON(1);
	}
	first = prhtn->llist.first;
	while (first) {
		prhn = container_of(first, struct preempt_rh_node, llist);
		if (WARN_ON_ONCE(prhn->frame != frame)) {
			break;
		}
		if (prhn->post_hook) {
			prhn->post_hook(prhn->data, correct_ret_addr, regs);
		}
		if (first == node) {
			break;
		}
		first = first->next;
	}

	/* Fixup registers for returning to correct address. */
	preempt_rethook_fixup_return(regs, correct_ret_addr);

	/* Unlink used shadow stack */
	if (prhtn) {
		first = prhtn->llist.first;
		prhtn->llist.first = node->next;
		node->next = NULL;
	}

	while (first) {
		prhn = container_of(first, struct preempt_rh_node, llist);
		first = first->next;
		preempt_rethook_recycle(prhn);
	}

	if (prhtn) {
		if (!prhtn->llist.first) {
			spin_lock(&preempt_rh_task_nodes_lock);
			hash_del(&prhtn->node);
			spin_unlock(&preempt_rh_task_nodes_lock);
			kfree(prhtn);
		}
	}

	return correct_ret_addr;
}

