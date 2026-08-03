#include "ftrace_hooking.h"
#include "tracer.h"
#include <asm/syscall.h>
#include <linux/kprobes.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,9,0)
#ifdef MOOCBT_IBT_SUPPORT
	#define USE_PATH_MOUNT_PREEMPT_RETHOOK
	#define USE_PATH_UMOUNT_PREEMPT_RETHOOK
#else //MOOCBT_IBT_SUPPORT
        #define USE_PATH_MOUNT
        #define USE_PATH_UMOUNT
#endif //MOOCBT_IBT_SUPPORT
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5,5,0)
        #define USE_DO_MOUNT
        #define USE_KSYS_UMOUNT
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
        #define USE_KSYS_MOUNT
        #define USE_KSYS_UMOUNT
#else
        #define USE_SYS_MOUNT
        #define USE_SYS_UMOUNT
#ifdef HAVE_SYS_OLDUMOUNT
        #define USE_SYS_OLDUMOUNT
#endif //HAVE_SYS_OLDUMOUNT
#endif //LINUX_VERSION_CODE

// do_move_mount() is static and its kallsyms name is unstable, so hook the
// arch's syscall wrapper which has a stable name.
// The hook passes kernel-space strings to handle_bdev_mount_event(), so it
// also requires a kernel where the mount handlers receive kernel strings,
// indicated by USE_PATH_MOUNT or USE_DO_MOUNT.
#ifdef HAVE_SYS_MOVE_MOUNT
	#if defined(CONFIG_X86_64)
                #define SYS_MOVE_MOUNT_SYMBOL "__x64_sys_move_mount"
		#ifdef USE_PATH_MOUNT_PREEMPT_RETHOOK
			#define USE_SYS_MOVE_MOUNT_PREEMPT_RETHOOK
		#elif defined(USE_PATH_MOUNT) || defined(USE_DO_MOUNT)
         		#define USE_SYS_MOVE_MOUNT
		#endif //USE_PATH_MOUNT_PREEMPT_RETHOOK
	#else
                #pragma message "disabling move_mount hook, no syscall wrapper for this arch"
	#endif //CONFIG_X86_64
#endif //HAVE_SYS_MOVE_MOUNT

#ifdef USE_HOOK_TRACER
// No-op function for preventing __submit_bio from executing when bios are
// deferred by COW operations
static void notrace moocbt___submit_bio_noop(struct bio *bio) {
	(void) bio;
}

static void notrace ftrace___submit_bio(unsigned long ip, unsigned long parent_ip,
		struct ftrace_ops *ops, struct ftrace_regs *fregs) {
	struct bio *bio = (struct bio *) ftrace_regs_get_argument(fregs, 0);

	if (moocbt_trace_bio(bio)) {
		// a non-zero return value indicates the bio must not be submitted
		// so COW operations happen in the correct order, and so that the
		// bio is not double-submitted.
		struct pt_regs *regs = ftrace_get_regs(fregs);
		regs->ip = (unsigned long) moocbt___submit_bio_noop;
	}
}
#endif //USE_HOOK_TRACER

#ifdef USE_PATH_MOUNT_PREEMPT_RETHOOK

struct path_mount_ctx {
	struct path *path;
	char *dir_name;
	char *buf;
	unsigned int idx;
	int ret;
};

static void path_mount_rh_remount_ro_post(void *data, unsigned long ip,
		struct pt_regs *regs) {
	struct path_mount_ctx *ctx = (struct path_mount_ctx *) data;
	int sys_ret = (int) regs_return_value(regs);
	post_umount_check(ctx->ret, sys_ret, ctx->idx, ctx->dir_name);
	kfree(ctx->buf);
}

static void path_mount_rh_mount_rw_post(void *data, unsigned long ip,
		struct pt_regs *regs) {
	struct path_mount_ctx *ctx = (struct path_mount_ctx *) data;
	int sys_ret = (int) regs_return_value(regs);
        if (!sys_ret) {
                ctx->dir_name = d_path(ctx->path, ctx->buf, PATH_MAX);    
                ctx->ret = handle_bdev_mounted_writable(ctx->dir_name,
				&ctx->idx);
        }
	kfree(ctx->buf);
}

static void ftrace_path_mount_preempt_rh(unsigned long ip,
		unsigned long parent_ip, struct ftrace_ops *ops,
		struct ftrace_regs *fregs) {
	struct path_mount_ctx ctx;
	struct path *path = (struct path *) ftrace_regs_get_argument(fregs, 1);
	unsigned long flags = ftrace_regs_get_argument(fregs, 3);
        unsigned long real_flags = flags;

        LOG_DEBUG("hook triggered: %s", __func__);

	ctx.path = path;
        ctx.buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if (!ctx.buf) {
		LOG_ERROR(-ENOMEM, "failed to allocate path buf");
                return;
        }

        // get rid of the magic value if it's present 
        if ((real_flags & MS_MGC_MSK) == MS_MGC_VAL) {
                real_flags &= ~MS_MGC_MSK;
	}

   	if (real_flags & (MS_BIND | MS_SHARED | MS_PRIVATE | MS_SLAVE |
                          MS_UNBINDABLE | MS_MOVE) ||
                ((real_flags & MS_RDONLY) && !(real_flags & MS_REMOUNT))) {
                // bind, shared, move, or new read-only mounts it do not affect
                // the state of the driver
		kfree(ctx.buf);
        } else if ((real_flags & MS_RDONLY) && (real_flags & MS_REMOUNT)) {
                // we are remounting read-only, same as umounting as far as the
                // driver is concerned
                ctx.dir_name = d_path(ctx.path, ctx.buf, PATH_MAX);
                ctx.ret = handle_bdev_mount_nowrite(ctx.dir_name, 0, &ctx.idx);
		struct pt_regs *regs = ftrace_get_regs(fregs);
		struct preempt_rethook prh = {
			.post_hook = path_mount_rh_remount_ro_post,
			.data_size = sizeof(struct path_mount_ctx),
			.data = (void*) &ctx,
		};
		if (pre_handler_preempt_rethook(&prh, regs)) {
			LOG_ERROR(-ENOMEM, "failed to setup path_mount post hook");
			post_umount_check(ctx.ret, 1, ctx.idx, ctx.dir_name);
			kfree(ctx.buf);
		}
        } else {
                // new read-write mount
		struct pt_regs *regs = ftrace_get_regs(fregs);
		struct preempt_rethook prh = {
			.post_hook = path_mount_rh_mount_rw_post,
			.data_size = sizeof(struct path_mount_ctx),
			.data = (void*) &ctx,
		};
		if (pre_handler_preempt_rethook(&prh, regs)) {
			LOG_ERROR(-ENOMEM, "failed to setup path_mount post hook");
			kfree(ctx.buf);
		}
        }
}

#endif //USE_PATH_MOUNT_PREEMPT_RETHOOK

#ifdef USE_PATH_MOUNT
static int (*orig_path_mount)(const char *dev_name, struct path *path,
		const char *type_page, unsigned long flags, void *data_page);

static int ftrace_path_mount(const char *dev_name, struct path *path,
		const char *type_page, unsigned long flags, void *data_page)

{
	int ret = 0;
        int sys_ret = 0;
        unsigned int idx = 0;
        unsigned long real_flags = flags;
        char *dir_name = NULL;
        char *buf = NULL;

        LOG_DEBUG("hook triggered: %s", __func__);

        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if (!buf) {
                return -ENOMEM;
        }

        // get rid of the magic value if its present 
        if ((real_flags & MS_MGC_MSK) == MS_MGC_VAL)
                real_flags &= ~MS_MGC_MSK;

   	if (real_flags & (MS_BIND | MS_SHARED | MS_PRIVATE | MS_SLAVE |
                          MS_UNBINDABLE | MS_MOVE) ||
                ((real_flags & MS_RDONLY) && !(real_flags & MS_REMOUNT))) {
                // bind, shared, move, or new read-only mounts it do not affect
                // the state of the driver
  
                sys_ret = orig_path_mount(dev_name, path, type_page, flags, data_page);
        } else if ((real_flags & MS_RDONLY) && (real_flags & MS_REMOUNT)) {
                // we are remounting read-only, same as umounting as far as the
                // driver is concerned
                dir_name = d_path(path, buf, PATH_MAX);
                ret = handle_bdev_mount_nowrite(dir_name, 0, &idx);

                sys_ret = orig_path_mount(dev_name, path, type_page, flags, data_page);
                post_umount_check(ret, sys_ret, idx, dir_name);
        } else {
                // new read-write mount
                sys_ret = orig_path_mount(dev_name, path, type_page, flags, data_page);

                if (!sys_ret) {
                        dir_name = d_path(path, buf, PATH_MAX);    
                        ret = handle_bdev_mounted_writable(dir_name, &idx);
                }
        }

        if(buf)
                kfree(buf);
        return sys_ret;
}
#endif //USE_PATH_MOUNT

#ifdef USE_DO_MOUNT
static long (*orig_do_mount)(const char *dev_name, const char __user *dir_name,
		const char *type_page, unsigned long flags, void *data_page);

static long ftrace_do_mount(const char *dev_name, const char __user *dir_name,
		const char *type_page, unsigned long flags, void *data_page)
{
	long ret = 0;
        long sys_ret;
        unsigned int idx = 0;
        unsigned long real_flags = flags;

        LOG_DEBUG("hook triggered: %s", __func__);
           
        // get rid of the magic value if its present
        if ((real_flags & MS_MGC_MSK) == MS_MGC_VAL)
                real_flags &= ~MS_MGC_MSK;

        if (real_flags & (MS_BIND | MS_SHARED | MS_PRIVATE | MS_SLAVE |
                          MS_UNBINDABLE | MS_MOVE) ||
            ((real_flags & MS_RDONLY) && !(real_flags & MS_REMOUNT))) {
                // bind, shared, move, or new read-only mounts it do not affect
                // the state of the driver
                sys_ret = orig_do_mount(dev_name, dir_name, type_page, flags, data_page);
        } else if ((real_flags & MS_RDONLY) && (real_flags & MS_REMOUNT)) {
                // we are remounting read-only, same as umounting as far as the
                // driver is concerned

                ret = handle_bdev_mount_nowrite(dir_name, 0, &idx);                
                sys_ret = orig_do_mount(dev_name, dir_name, type_page, flags, data_page);
                post_umount_check(ret, sys_ret, idx, dir_name);
        } else {
                // new read-write mount
                sys_ret = orig_do_mount(dev_name, dir_name, type_page, flags, data_page);
                if (!sys_ret) {
                        handle_bdev_mounted_writable(dir_name, &idx);
                }
        }

        return sys_ret;
}
#endif //USE_DO_MOUNT

#ifdef USE_KSYS_MOUNT
static int (*orig_ksys_mount)(char __user *dev_name, char __user *dir_name, char __user *type,
	       unsigned long flags, void __user *data);

static int ftrace_ksys_mount(char __user *dev_name, char __user *dir_name, char __user *type,
	       unsigned long flags, void __user *data)
{
	long ret = 0;
        long sys_ret = 0;
        unsigned int idx = 0;
        unsigned long real_flags = flags;

        LOG_DEBUG("hook triggered: %s", __func__);
 
        // get rid of the magic value if its present
        if ((real_flags & MS_MGC_MSK) == MS_MGC_VAL)
                real_flags &= ~MS_MGC_MSK;

        if (real_flags & (MS_BIND | MS_SHARED | MS_PRIVATE | MS_SLAVE |
                          MS_UNBINDABLE | MS_MOVE) ||
            ((real_flags & MS_RDONLY) && !(real_flags & MS_REMOUNT))) {
                // bind, shared, move, or new read-only mounts it do not affect
                // the state of the driver
                sys_ret = orig_ksys_mount(dev_name, dir_name, type, flags, data);
        } else if ((real_flags & MS_RDONLY) && (real_flags & MS_REMOUNT)) {
                // we are remounting read-only, same as umounting as far as the
                // driver is concerned
                ret = handle_bdev_mount_nowrite(dir_name, 0, &idx);
                sys_ret = orig_ksys_mount(dev_name, dir_name, type, flags, data);
                post_umount_check(ret, sys_ret, idx, dir_name);
        } else {
                // new read-write mount
                sys_ret = orig_ksys_mount(dev_name, dir_name, type, flags, data);
                if (!sys_ret)
                        handle_bdev_mounted_writable(dir_name, &idx);
        }

        return sys_ret;
}
#endif //USE_KSYS_MOUNT

#ifdef USE_SYS_MOUNT
static asmlinkage long (*orig_sys_mount)(char __user *dev_name, char __user *dir_name,
				char __user *type, unsigned long flags,
				void __user *data);

static asmlinkage long ftrace_sys_mount(char __user *dev_name, char __user *dir_name,
				char __user *type, unsigned long flags,
				void __user *data)
{
        int ret = 0;
        long sys_ret = 0;
        unsigned int idx = 0;
        unsigned long real_flags = flags;

        LOG_DEBUG("hook triggered: %s", __func__);

        // get rid of the magic value if its present
        if ((real_flags & MS_MGC_MSK) == MS_MGC_VAL)
                real_flags &= ~MS_MGC_MSK;

        if (real_flags & (MS_BIND | MS_SHARED | MS_PRIVATE | MS_SLAVE |
                          MS_UNBINDABLE | MS_MOVE) ||
            ((real_flags & MS_RDONLY) && !(real_flags & MS_REMOUNT))) {
                // bind, shared, move, or new read-only mounts it do not affect
                // the state of the driver
                sys_ret = orig_sys_mount(dev_name, dir_name, type, flags, data);
        } else if ((real_flags & MS_RDONLY) && (real_flags & MS_REMOUNT)) {
                // we are remounting read-only, same as umounting as far as the
                // driver is concerned
                ret = handle_bdev_mount_nowrite(dir_name, 0, &idx);
                sys_ret = orig_sys_mount(dev_name, dir_name, type, flags, data);
                post_umount_check(ret, sys_ret, idx, dir_name);
        } else {
                // new read-write mount
                sys_ret = orig_sys_mount(dev_name, dir_name, type, flags, data);
                if (!sys_ret)
                        handle_bdev_mounted_writable(dir_name, &idx);
        }

        return sys_ret;
}
#endif //USE_SYS_MOUNT

#ifdef USE_SYS_MOVE_MOUNT_PREEMPT_RETHOOK

struct move_mount_ctx {
	int to_dfd;
	const char __user *to_pathname;
	unsigned int flags;
};

static void move_mount_rh_post(void *data, unsigned long ip,
		struct pt_regs *regs) {
	struct move_mount_ctx *ctx = (struct move_mount_ctx *) data;
	int sys_ret = (int) regs_return_value(regs);
	unsigned int idx = 0;
	unsigned int lookup_flags = 0;
        struct path path;
	char *buf;
	char *dir_name;

	if (sys_ret) {
		return;
	}
        if (!(ctx->flags & MOVE_MOUNT_F_EMPTY_PATH)) {
                // Moving a currently attached mount to another location does
                // not change the state of the driver.
		return;
	}
#ifdef MOVE_MOUNT_BENEATH
	// MOVE_MOUNT_BENEATH was added in kernel 6.5
	if (ctx->flags & MOVE_MOUNT_BENEATH) {
		// A mount attached beneath an existing mount does not change
		// which filesystem is visible at the mount point, so it does
		// not change the state of the driver.
		return;
	}
#endif //MOVE_MOUNT_BENEATH
        if (ctx->flags & MOVE_MOUNT_T_SYMLINKS) {
                lookup_flags |= LOOKUP_FOLLOW;
        }
        if (ctx->flags & MOVE_MOUNT_T_AUTOMOUNTS) {
                lookup_flags |= LOOKUP_AUTOMOUNT;
        }
        if (ctx->flags & MOVE_MOUNT_T_EMPTY_PATH) {
                lookup_flags |= LOOKUP_EMPTY;
        }

        if (user_path_at(ctx->to_dfd, ctx->to_pathname, lookup_flags, &path)) {
                return;
        }

        // read-only mounts do not affect the state of the driver
        if (!((path.mnt->mnt_flags & MNT_READONLY) ||
			(path.mnt->mnt_sb->s_flags & MS_RDONLY))) {
                buf = kmalloc(PATH_MAX, GFP_KERNEL);
                if (buf) {
                        dir_name = d_path(&path, buf, PATH_MAX);
                        if (!IS_ERR(dir_name)) {
                                handle_bdev_mounted_writable(dir_name, &idx);
                        }
                        kfree(buf);
                }
        }
        path_put(&path);
}

static void ftrace_move_mount_preempt_rh(unsigned long ip,
		unsigned long parent_ip, struct ftrace_ops *ops,
		struct ftrace_regs *fregs) {
	struct move_mount_ctx ctx;
	struct pt_regs *regs = (struct pt_regs *) ftrace_regs_get_argument(fregs, 0);
	unsigned long args[6];

        LOG_DEBUG("hook triggered: %s", __func__);

        // move_mount(from_dfd, from_pathname, to_dfd, to_pathname, flags)
        syscall_get_arguments(current, regs, args);
        ctx.to_dfd = (int) args[2];
        ctx.to_pathname = (const char __user *) args[3];
        ctx.flags = (unsigned int) args[4];

	struct pt_regs *trace_regs = ftrace_get_regs(fregs);
	struct preempt_rethook prh = {
		.post_hook = move_mount_rh_post,
		.data_size = sizeof(struct move_mount_ctx),
		.data = (void*) &ctx,
	};
	if (pre_handler_preempt_rethook(&prh, trace_regs)) {
		LOG_ERROR(-ENOMEM, "failed to setup sys_move_mount post hook");
	}
}

#endif //USE_SYS_MOVE_MOUNT_PREEMPT_RETHOOK

#ifdef USE_SYS_MOVE_MOUNT
static asmlinkage long (*orig_sys_move_mount)(struct pt_regs *regs);

static asmlinkage long ftrace_sys_move_mount(struct pt_regs *regs)
{
        long sys_ret;
        unsigned int idx = 0;
        unsigned int lookup_flags = 0;
        int to_dfd;
        const char __user *to_pathname;
        unsigned int flags;
        struct path path;
        char *dir_name;
        char *buf = NULL;
        unsigned long args[6];

        LOG_DEBUG("hook triggered: %s", __func__);

        // move_mount(from_dfd, from_pathname, to_dfd, to_pathname, flags)
        syscall_get_arguments(current, regs, args);
        to_dfd = (int)args[2];
        to_pathname = (const char __user *)args[3];
        flags = (unsigned int)args[4];

        sys_ret = orig_sys_move_mount(regs);
        if (sys_ret) {
                return sys_ret;
        }

        if (!(flags & MOVE_MOUNT_F_EMPTY_PATH)) {
                // Moving a currently attached mount to another location does
                // not change the state of the driver.
                return sys_ret;
        }
#ifdef MOVE_MOUNT_BENEATH
        // MOVE_MOUNT_BENEATH was added in kernel 6.5
        if (flags & MOVE_MOUNT_BENEATH) {
                // A mount attached beneath an existing mount does not change
                // which filesystem is visible at the mount point, so it does
                // not change the state of the driver.
                return sys_ret;
        }
#endif //MOVE_MOUNT_BENEATH

        if (flags & MOVE_MOUNT_T_SYMLINKS) {
                lookup_flags |= LOOKUP_FOLLOW;
        }
        if (flags & MOVE_MOUNT_T_AUTOMOUNTS) {
                lookup_flags |= LOOKUP_AUTOMOUNT;
        }
        if (flags & MOVE_MOUNT_T_EMPTY_PATH) {
                lookup_flags |= LOOKUP_EMPTY;
        }

        if (user_path_at(to_dfd, to_pathname, lookup_flags, &path)) {
                return sys_ret;
        }

        // read-only mounts do not affect the state of the driver
        if (!((path.mnt->mnt_flags & MNT_READONLY) ||
                (path.mnt->mnt_sb->s_flags & MS_RDONLY)))
        {
                buf = kmalloc(PATH_MAX, GFP_KERNEL);
                if (buf) {
                        dir_name = d_path(&path, buf, PATH_MAX);
                        if (!IS_ERR(dir_name)) {
                                handle_bdev_mounted_writable(dir_name, &idx);
                        }
                        kfree(buf);
                }
        }

        path_put(&path);
        return sys_ret;
}
#endif //USE_SYS_MOVE_MOUNT

#ifdef USE_PATH_UMOUNT_PREEMPT_RETHOOK

struct path_umount_ctx {
	struct path *path;
	char *dir_name;
	char *buf;
	unsigned int idx;
	int ret;
};

static void path_umount_rh_post(void *data, unsigned long ip,
		struct pt_regs *regs) {
	struct path_umount_ctx *ctx = (struct path_umount_ctx *) data;
	int sys_ret = (int) regs_return_value(regs);
        ctx->dir_name = d_path(ctx->path, ctx->buf, PATH_MAX);
        post_umount_check(ctx->ret, sys_ret, ctx->idx, ctx->dir_name);
        kfree(ctx->buf);
}

static void ftrace_path_umount_preempt_rh(unsigned long ip,
		unsigned long parent_ip, struct ftrace_ops *ops,
		struct ftrace_regs *fregs) {
	struct path_umount_ctx ctx;
	struct path *path = (struct path *) ftrace_regs_get_argument(fregs, 0);
	unsigned long flags = ftrace_regs_get_argument(fregs, 1);
        unsigned long real_flags = flags;

        LOG_DEBUG("hook triggered: %s", __func__);

	ctx.path = path;
        ctx.buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if (!ctx.buf) {
		LOG_ERROR(-ENOMEM, "failed to allocate path buf");
                return;
        }

        // get rid of the magic value if its present
        if ((real_flags & MS_MGC_MSK) == MS_MGC_VAL) {
		real_flags &= ~MS_MGC_MSK;
	}

        ctx.dir_name = d_path(ctx.path, ctx.buf, PATH_MAX);
        ctx.ret = handle_bdev_mount_nowrite(ctx.dir_name, real_flags, &ctx.idx);

	struct pt_regs *regs = ftrace_get_regs(fregs);
	struct preempt_rethook prh = {
		.post_hook = path_umount_rh_post,
		.data_size = sizeof(struct path_umount_ctx),
		.data = (void*) &ctx,
	};
	if (pre_handler_preempt_rethook(&prh, regs)) {
		LOG_ERROR(-ENOMEM, "failed to setup path_umount post hook");
		post_umount_check(ctx.ret, 1, ctx.idx, ctx.dir_name);
		kfree(ctx.buf);
	}
}

#endif //USE_PATH_UMOUNT_PREEMPT_RETHOOK

#ifdef USE_PATH_UMOUNT
static int (*orig_path_umount)(struct path *path, int flags);

static int ftrace_path_umount(struct path *path, int flags)
{
      	int ret = 0;
        int sys_ret = 0;
        unsigned int idx = 0;
        char *dir_name = NULL;
        char *buf;
        int real_flags = flags;

        LOG_DEBUG("hook triggered: %s", __func__);

        // get rid of the magic value if its present
        if ((real_flags & MS_MGC_MSK) == MS_MGC_VAL)
		real_flags &= ~MS_MGC_MSK;

        buf = kmalloc(PATH_MAX, GFP_KERNEL);
        if (!buf) {
                return -ENOMEM;
        }
 
        dir_name = d_path(path, buf, PATH_MAX);
        ret = handle_bdev_mount_nowrite(dir_name, real_flags, &idx);

        sys_ret = orig_path_umount(path, flags);
        
        dir_name = d_path(path, buf, PATH_MAX);
        post_umount_check(ret, sys_ret, idx, dir_name);

        if(buf)
                kfree(buf);

        return sys_ret;
}
#endif //USE_PATH_UMOUNT

#ifdef USE_KSYS_UMOUNT
static int (*orig_ksys_umount)(char __user *name, int flags);

static int ftrace_ksys_umount(char __user *name, int flags)
{
	int ret = 0;
        int sys_ret = 0;
        unsigned int idx = 0;

        LOG_DEBUG("hook triggered: %s", __func__);
        
        ret = handle_bdev_mount_nowrite(name, flags, &idx);

        sys_ret = orig_ksys_umount(name, flags);
        post_umount_check(ret, sys_ret, idx, name);

        return sys_ret;
}
#endif //USE_KSYS_UMOUNT

#ifdef USE_SYS_UMOUNT
static asmlinkage long (*orig_sys_umount)(char __user *name, int flags);

static asmlinkage long ftrace_sys_umount(char __user *name, int flags)
{
        int ret = 0;
        long sys_ret = 0;
        unsigned int idx = 0;

        LOG_DEBUG("hook triggered: %s", __func__);

        ret = handle_bdev_mount_nowrite(name, flags, &idx);
        sys_ret = orig_sys_umount(name, flags);
        post_umount_check(ret, sys_ret, idx, name);

        return sys_ret;
}
#endif //USE_SYS_UMOUNT

#ifdef USE_SYS_OLDUMOUNT
asmlinkage long (*orig_sys_oldumount)(char __user *name);

asmlinkage long ftrace_sys_oldumount(char __user *name)
{
        int ret;
        long sys_ret;
        unsigned int idx;

        LOG_DEBUG("hook triggered: %s", __func__);
       
        ret = handle_bdev_mount_nowrite(name, 0, &idx);
        sys_ret = orig_sys_oldumount(name);
        post_umount_check(ret, sys_ret, idx, name);

        return sys_ret;
}
#endif //USE_SYS_OLDUMOUNT

static struct ftrace_hook ftrace_hooks[] = {
#ifdef USE_HOOK_TRACER
	HOOK("__submit_bio", ftrace___submit_bio, NULL, FTRACE_OPS_FL_RECURSION | FTRACE_OPS_FL_IPMODIFY, false),
#endif //USE_HOOK_TRACER

#ifdef USE_PATH_MOUNT_PREEMPT_RETHOOK
	HOOK("path_mount", ftrace_path_mount_preempt_rh, NULL, 0, false),
#endif //USE_PATH_MOUNT_PREEMPT_RETHOOK

#ifdef USE_PATH_UMOUNT_PREEMPT_RETHOOK
	HOOK("path_umount", ftrace_path_umount_preempt_rh, NULL, 0, false),
#endif //USE_PATH_UMOUNT_PREEMPT_RETHOOK

#ifdef USE_PATH_MOUNT
        HOOK("path_mount", ftrace_path_mount, &orig_path_mount, FTRACE_OPS_FL_RECURSION | FTRACE_OPS_FL_IPMODIFY, true),
#endif //USE_PATH_MOUNT

#ifdef USE_PATH_UMOUNT
        HOOK("path_umount", ftrace_path_umount, &orig_path_umount, FTRACE_OPS_FL_RECURSION | FTRACE_OPS_FL_IPMODIFY, true),
#endif //USE_PATH_UMOUNT

#ifdef USE_SYS_MOVE_MOUNT_PREEMPT_RETHOOK
	HOOK(SYS_MOVE_MOUNT_SYMBOL, ftrace_move_mount_preempt_rh, NULL, 0, false)
#endif //USE_SYS_MOVE_MOUNT_PREEMPT_RETHOOK

#ifdef USE_SYS_MOVE_MOUNT
        HOOK(SYS_MOVE_MOUNT_SYMBOL, ftrace_sys_move_mount, &orig_sys_move_mount, FTRACE_OPS_FL_RECURSION | FTRACE_OPS_FL_IPMODIFY, true),
#endif //USE_SYS_MOVE_MOUNT

#ifdef USE_DO_MOUNT
        HOOK("do_mount", ftrace_do_mount, &orig_do_mount, FTRACE_OPS_FL_RECURSION | FTRACE_OPS_FL_IPMODIFY, true),
#endif //USE_DO_MOUNT

#ifdef USE_KSYS_MOUNT
        HOOK("ksys_mount", ftrace_ksys_mount, &orig_ksys_mount, FTRACE_OPS_FL_RECURSION | FTRACE_OPS_FL_IPMODIFY, true),
#endif //USE_KSYS_MOUNT

#ifdef USE_KSYS_UMOUNT
        HOOK("ksys_umount", ftrace_ksys_umount, &orig_ksys_umount, FTRACE_OPS_FL_RECURSION | FTRACE_OPS_FL_IPMODIFY, true),
#endif //USE_KSYS_UMOUNT

#ifdef USE_SYS_MOUNT
        HOOK("sys_mount", ftrace_sys_mount, &orig_sys_mount, FTRACE_OPS_FL_RECURSION | FTRACE_OPS_FL_IPMODIFY, true),
#endif //USE_SYS_MOUNT

#ifdef USE_SYS_UMOUNT
        HOOK("sys_umount", ftrace_sys_umount, &orig_sys_umount, FTRACE_OPS_FL_RECURSION | FTRACE_OPS_FL_IPMODIFY, true),
#endif //USE_SYS_UMOUNT

#ifdef USE_SYS_OLDUMOUNT
        HOOK("sys_oldumount", ftrace_sys_oldumount, &orig_sys_oldumount, FTRACE_OPS_FL_RECURSION | FTRACE_OPS_FL_IPMODIFY, true),
#endif //USE_SYS_OLDUMOUNT
};

// Needs CONFIG_KPROBES=y as well as CONFIG_KALLSYMS=y
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
static unsigned long lookup_name(const char *name)
{
	struct kprobe kp = {
		.symbol_name = name
	};
	unsigned long address = 0;
	int ret = 0;

	ret = register_kprobe(&kp);

	if (ret < 0) 
	{
		LOG_ERROR(ret, "failed registering kprobe for %s", name);
		return 0;
	}
	address = (unsigned long)kp.addr;
	unregister_kprobe(&kp);

	return address;
}
#else
static unsigned long lookup_name(const char *name)
{
	unsigned long address;
	address = kallsyms_lookup_name(name);

	return address;
}
#endif //LINUX_VERSION_CODE

static int resolve_hook_address(struct ftrace_hook *hook)
{
	hook->address = lookup_name(hook->name);

	if (!hook->address) {
		LOG_ERROR(-ENOENT, "unresolved symbol: %s", hook->name);
		return -ENOENT;
	}

	if (hook->original) {
#if USE_FENTRY_OFFSET
		*((unsigned long*) hook->original) = hook->address + MCOUNT_INSN_SIZE;
#else
		*((unsigned long*) hook->original) = hook->address;
#endif //USE_FENTRY_OFFSET
	}

	return 0;
}

static inline bool moocbt_within_module(unsigned long addr,
		const struct module *mod) {
#ifdef HAVE_WITHIN_MODULE
        return within_module(addr, mod);
#else
        return within_module_init(addr, mod) || within_module_core(addr, mod);
#endif
}

static void notrace ftrace_callback_handler(unsigned long ip, unsigned long parent_ip,
		struct ftrace_ops *ops, struct ftrace_regs *fregs)
{
	struct ftrace_hook *hook = container_of(ops, struct ftrace_hook, ops);
	if (hook->direct_hook_call && ops->flags & FTRACE_OPS_FL_IPMODIFY) {
		struct pt_regs *regs = ftrace_get_regs(fregs);
#if USE_FENTRY_OFFSET
		regs->ip = (unsigned long)hook->function;
#else //USE_FENTRY_OFFSET
		if (!moocbt_within_module(parent_ip, THIS_MODULE)) {
			regs->ip = (unsigned long)hook->function;
		}
#endif //USE_FENTRY_OFFSET
	} else {
		void (*function)(unsigned long, unsigned long,
				struct ftrace_ops *, struct ftrace_regs *) = 
			(void (*)(unsigned long, unsigned long,
				struct ftrace_ops *, struct ftrace_regs *)) hook->function;
		if (!moocbt_within_module(parent_ip, THIS_MODULE)) {
			function(ip, parent_ip, ops, fregs);
		}
	}
}

/**
 * register_hook() - registers and enables a single hook
 * @hook: a hook to install
 * 
 * Return:
 * 0 - success
 * !0 - an errno indicating the error
 */
static int register_hook(struct ftrace_hook *hook)
{
	int ret = 0;

	ret = resolve_hook_address(hook);
	if (ret)
	{
		LOG_ERROR(ret, "failed resolving hook address for %s", hook->name);
		return ret;
	}
	
	hook->ops.func = ftrace_callback_handler;
	hook->ops.flags |= FTRACE_OPS_FL_SAVE_REGS | hook->op_flags;

	ret = ftrace_set_filter_ip(&hook->ops, hook->address, 0, 0);
	if (ret) {
		LOG_ERROR(ret, "failed setting ftrace filter ip: %d for %s", ret, hook->name);
		return ret;
	}

	ret = register_ftrace_function(&hook->ops);
	if (ret) {
		LOG_ERROR(ret, "failed registering ftrace function for %s", hook->name);
		ftrace_set_filter_ip(&hook->ops, hook->address, 1, 0);
		return ret;
	}

        LOG_DEBUG("registered ftrace hook for %s", hook->name);

	return ret;
}

/**
 * unregister_hook() - disable and unregister a single hook
 * @hook: a hook to remove
 * 
 * Return:
 * 0 - success
 * !0 - an errno indicating the error
 */
static int unregister_hook(struct ftrace_hook *hook)
{
	int ret = 0;

	ret = unregister_ftrace_function(&hook->ops);
	if (ret) {
		LOG_ERROR(ret, "failed unregistering ftrace function for %s", hook->name);
	}

	ret = ftrace_set_filter_ip(&hook->ops, hook->address, 1, 0);
	if (ret) {
		LOG_ERROR(ret, "failed setting ftrace filter ip for %s", hook->name);
	}
        return ret;
}

int register_ftrace_hooks(void)
{
	int ret = 0;
	int i;
	int count = ARRAY_SIZE(ftrace_hooks);

	for (i = 0; i < count; i++) {
		ret = register_hook(&ftrace_hooks[i]);
		if (ret)
			goto error;
	}

	return 0;
error:
	while (i != 0) {
		unregister_hook(&ftrace_hooks[--i]);
	}
	return ret;
}

int unregister_ftrace_hooks(void)
{
	int ret = 0;
	int i;
	int count = ARRAY_SIZE(ftrace_hooks);

	for (i = 0; i < count; i++) {
		unregister_hook(&ftrace_hooks[i]);
	}

	return ret;
}
