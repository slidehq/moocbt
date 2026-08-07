#include "cow_manager.h"
#include "moocbt.h"
#include "includes.h"
#include "ioctl_handlers.h"
#include "module_control.h"
#include "snap_device.h"
#include "tracer_helper.h"
#include "proc_seq_file.h"

#ifdef CONFIG_X86
#include <asm/processor.h>
#include <asm/cpufeature.h>
#endif //CONFIG_X86

static void *moocbt_proc_start(struct seq_file *m, loff_t *pos);
static void *moocbt_proc_next(struct seq_file *m, void *v, loff_t *pos);
static void moocbt_proc_stop(struct seq_file *m, void *v);
static int moocbt_proc_show(struct seq_file *m, void *v);
static int moocbt_proc_open(struct inode *inode, struct file *filp);
static int moocbt_proc_release(struct inode *inode, struct file *file);

#ifndef HAVE_PROC_OPS
//#if LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)
static const struct file_operations moocbt_proc_fops = {
        .owner = THIS_MODULE,
        .open = moocbt_proc_open,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = moocbt_proc_release,
};
#else
static const struct proc_ops moocbt_proc_fops = {
        .proc_open = moocbt_proc_open,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = moocbt_proc_release,
};
#endif

static const struct seq_operations moocbt_seq_proc_ops = {
        .start = moocbt_proc_start,
        .next = moocbt_proc_next,
        .stop = moocbt_proc_stop,
        .show = moocbt_proc_show,
};

#ifndef HAVE_PROC_OPS

/**
 * get_proc_fops() - Retrieves a file operations struct pointer
 *
 * Return:
 * The &struct file_operations object pointer.
 */
const struct file_operations* get_proc_fops(void)
{
        return &moocbt_proc_fops;
}

#else // HAVE_PROC_OPS

/**
 * get_proc_fops() - Retrieves a file operations struct pointer
 *
 * Return:
 * The &struct file_operations object pointer.
 */
const struct proc_ops* get_proc_fops(void)
{
        return &moocbt_proc_fops;
}

#endif // HAVE_PROC_OPS

static snap_device_array current_snap_devices = NULL;

/**
 * moocbt_proc_get_idx() - Turns offset into pointer into @snap_devices array.
 * @pos: An offset into the array of @snap_devices.
 *
 * Return:
 * * NULL - invalid @pos supplied, indicates "past end of file."
 * * !NULL - a void* pointer into the @snap_devices array.
 */
static void *moocbt_proc_get_idx(loff_t pos)
{
        if (pos > highest_minor)
                return NULL;
        return (void*)&current_snap_devices[pos];
}

/**
 * moocbt_proc_start() - Prepares to iterate through the @snap_devices array.
 *
 * @m: Pointer to a seq_file structure.
 * @pos: the previous offset from the last iteration session.
 *
 * Return:
 * * NULL - @pos does not translate to a valid @snap_devices entry.
 * * SEQ_START_TOKEN - A new iteration from the start so print a header first.
 * * otherwise - Pointer into @snap_devices at offset @pos.
 */
static void *moocbt_proc_start(struct seq_file *m, loff_t *pos)
{
        /*
         * Depending on how much we've printed thus far our *_stop() might
         * be called followed by an invocation of this function with a non-
         * zero @pos with the expectation that we continue from where we
         * left off.
         */ 
        current_snap_devices = get_snap_device_array();
        if (*pos == 0)
                return SEQ_START_TOKEN;
        return moocbt_proc_get_idx(*pos - 1);
}

/**
 * moocbt_proc_next() - Return the next entry to *_show() and advance @pos.
 *
 * @m: The sequence file structure.
 * @v: The value last returned from *_start() or *_next()
 * @pos: The value passed in represents the position of the next item in
 *       the snap_devices array.  The value returned holds the position that
 *       start() could use to find the next snap_device.
 *
 * Return:
 * * NULL - @pos does not represent a valid entry in @snap_devices.
 * * otherwise - A pointer to the entry to *_show().
 */
static void *moocbt_proc_next(struct seq_file *m, void *v, loff_t *pos)
{
        void *dev = moocbt_proc_get_idx(*pos);
        ++*pos;
        return dev;
}

/** moocbt_proc_stop() - Always called at the end of iterating through the
 *                        @snap_devices array.
 * @m: The sequence file structure.
 * @v: The value last returned from *_start() or *_next()
 *
 * The end of iterating through the array is identified by a NULL return
 * value from either *_start() or *_next().
 */
static void moocbt_proc_stop(struct seq_file *m, void *v)
{
        put_snap_device_array(current_snap_devices);
        current_snap_devices = NULL;
}

static void moocbt_proc_ibt_show(struct seq_file *m) {
        int cpu_build_support = 0;
        int cpu_runtime_support = 0;
        int kernel_build_support = 0;
        int kernel_runtime_support = 0;
        int driver_support_force_on = 0;
        int driver_support_force_off = 0;
        int net_ibt_support_enabled = 0;
        int i = 0;

#ifdef BUILD_CPU_HAS_IBT
        cpu_build_support = 1;
#endif //BUILD_CPU_HAS_IBT
#ifdef CONFIG_X86
        // Read CPUID directly which reflects the physical CPU. Value is
        // independent of the kernel build config and runtime "ibt" parameter.
        // Leaf 7 only exists when the CPU reports a max basic leaf >= 7.
        // Older CPUs can return another leaf's data if it doesn't exist.
        if (boot_cpu_data.cpuid_level >= 7) {
                // CPUID.(EAX=7, ECX=0):EDX[20] is CET_IBT.
                unsigned int eax, ebx, ecx, edx;
                cpuid_count(7, 0, &eax, &ebx, &ecx, &edx);
                cpu_runtime_support = !!(edx & (1u << 20));
        }
#endif //CONFIG_X86
#ifdef CONFIG_X86_KERNEL_IBT
        kernel_build_support = 1;
#endif //CONFIG_X86_KERNEL_IBT
#if defined(CONFIG_X86) && defined(X86_FEATURE_IBT)
        kernel_runtime_support = !!cpu_feature_enabled(X86_FEATURE_IBT);
#endif //CONFIG_X86 && X86_FEATURE_IBT
#ifdef BUILD_CPU_HAS_IBT_FORCE_ON
        driver_support_force_on = 1;
#endif //BUILD_CPU_HAS_IBT_FORCE_ON
#ifdef BUILD_CPU_HAS_IBT_FORCE_OFF
        driver_support_force_off = 1;
#endif //BUILD_CPU_HAS_IBT_FORCE_OFF
#ifdef MOOCBT_IBT_SUPPORT
        net_ibt_support_enabled = 1;
#endif

        seq_printf(m, "\t\"ibt_status\": {\n");

#define STATUS(name, present) \
        seq_printf(m, "%s\t\t\"%s\": %s", (i++ ? ",\n" : ""), (name), (present) ? "true" : "false");

        STATUS("cpu_build_support", cpu_build_support);
        STATUS("cpu_runtime_support", cpu_runtime_support);
        STATUS("kernel_build_support", kernel_build_support);
        STATUS("kernel_runtime_support", kernel_runtime_support);
        STATUS("driver_support_force_on", driver_support_force_on);
        STATUS("driver_support_force_off", driver_support_force_off);
        STATUS("net_ibt_support_enabled", net_ibt_support_enabled);

#undef STATUS

        seq_printf(m, "\n\t},\n");

}

/** moocbt_proc_show() - Outputs information about a @snap_device.  Optionally
 *                        adds header and/or footer.
 * @m: The seq_file structure.
 * @v: The entry supplied from the last call to either *_start() or *_next().
 *
 * Return:
 * Always indicates success with a zero value.
 */
static int moocbt_proc_show(struct seq_file *m, void *v)
{
        struct snap_device **dev_ptr = v;
        struct snap_device *dev = NULL;

        // print the header if the "pointer" really an indication to do so
        if (dev_ptr == SEQ_START_TOKEN) {
                seq_printf(m, "{\n");
                seq_printf(m, "\t\"version\": \"%s\",\n", MOOCBT_VERSION);

                seq_printf(m, "\t\"build_features\": {\n");
                {
                        int i = 0;
#define FEATURE(name, present) \
        seq_printf(m, "%s\t\t\"%s\": %s", (i++ ? ",\n" : ""), (name), (present) ? "true" : "false");
                        MOOCBT_BUILD_FEATURE_LIST(FEATURE)
#undef FEATURE
                }
                seq_printf(m, "\n\t},\n");

                seq_printf(m, "\t\"build_symbols\": {\n");
                {
                        int i = 0;
#define SYMBOL(name, present) \
        seq_printf(m, "%s\t\t\"%s\": %s", (i++ ? ",\n" : ""), (name), (present) ? "true" : "false");
                        MOOCBT_BUILD_SYMBOL_LIST(SYMBOL)
#undef SYMBOL
                }
                seq_printf(m, "\n\t},\n");

                moocbt_proc_ibt_show(m);

                seq_printf(m, "\t\"devices\": [\n");
        }

        // if the pointer is actually a device print it
        if (dev_ptr != SEQ_START_TOKEN && *dev_ptr != NULL) {
                int error;
                dev = *dev_ptr;

                if (dev->sd_minor != lowest_minor)
                        seq_printf(m, ",\n");
                seq_printf(m, "\t\t{\n");
                seq_printf(m, "\t\t\t\"minor\": %u,\n", dev->sd_minor);
                seq_printf(m, "\t\t\t\"cow_file\": \"%s\",\n",
                           dev->sd_cow_path);
                seq_printf(m, "\t\t\t\"block_device\": \"%s\",\n",
                           dev->sd_bdev_path);
                seq_printf(m, "\t\t\t\"max_cache\": %lu,\n",
                           (dev->sd_cache_size) ?
                                   dev->sd_cache_size :
                                   moocbt_cow_max_memory_default);

                if (!test_bit(UNVERIFIED, &dev->sd_state)) {
                        seq_printf(m, "\t\t\t\"fallocate\": %llu,\n",
                                   ((unsigned long long)dev->sd_falloc_size) *
                                           1024 * 1024);

                        if (dev->sd_cow) {
                                int i;
                                seq_printf(m, "\t\t\t\"cow_size_current\": %llu,\n",
                                   (unsigned long long)dev->sd_cow->file_size);

                                seq_printf(
                                        m, "\t\t\t\"seq_id\": %llu,\n",
                                        (unsigned long long)dev->sd_cow->seqid);

                                seq_printf(m, "\t\t\t\"uuid\": \"");
                                for (i = 0; i < COW_UUID_SIZE; i++) {
                                        seq_printf(m, "%02x",
                                                   dev->sd_cow->uuid[i]);
                                }
                                seq_printf(m, "\",\n");

                                if (dev->sd_cow->version > COW_VERSION_0) {
                                        seq_printf(m,
                                                   "\t\t\t\"version\": %llu,\n",
                                                   dev->sd_cow->version);
                                        seq_printf(
                                                m,
                                                "\t\t\t\"nr_changed_blocks\": "
                                                "%llu,\n",
                                                dev->sd_cow->nr_changed_blocks);
                                }
                        }
                }

                error = tracer_read_fail_state(dev);
                if (error)
                        seq_printf(m, "\t\t\t\"error\": %d,\n", error);

                seq_printf(m, "\t\t\t\"state\": %lu\n", dev->sd_state);
                seq_printf(m, "\t\t}");
        }

        // print the footer if there are no devices to print or if this device
        // has the highest minor
        if ((dev_ptr == SEQ_START_TOKEN && lowest_minor > highest_minor) ||
            (dev && dev->sd_minor == highest_minor)) {
                seq_printf(m, "\n\t]\n");
                seq_printf(m, "}\n");
        }

        return 0;
}

static int moocbt_proc_open(struct inode *inode, struct file *filp)
{
        mutex_lock(&ioctl_mutex);
        return seq_open(filp, &moocbt_seq_proc_ops);
}

static int moocbt_proc_release(struct inode *inode, struct file *file)
{
        seq_release(inode, file);
        mutex_unlock(&ioctl_mutex);
        return 0;
}
