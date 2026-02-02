// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2022 Datto Inc.
 * Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
 */

#ifndef MODULE_CONTROL_H_
#define MODULE_CONTROL_H_

// name macros
#define INFO_PROC_FILE "moocbt-info"
#define DRIVER_NAME "moocbt"
#define CONTROL_DEVICE_NAME "moocbt-ctl"
#define SNAP_DEVICE_NAME "moocbt%u"
#define SNAP_COW_THREAD_NAME_FMT "moocbt_snap_cow%d"
#define SNAP_MRF_THREAD_NAME_FMT "moocbt_snap_mrf%d"
#define INC_THREAD_NAME_FMT "moocbt_inc%d"

// global module parameters
extern int moocbt_may_hook_syscalls;
extern unsigned long moocbt_cow_max_memory_default;
extern unsigned int moocbt_cow_fallocate_percentage_default;
extern unsigned int moocbt_max_snap_devices;

extern unsigned int highest_minor;
extern unsigned int lowest_minor;
extern int major;

#endif /* MODULE_CONTROL_H_ */
