/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * Copyright (C) 2015 Datto Inc.
 * Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
 */

#ifndef LIBMOOCBT_H_
#define LIBMOOCBT_H_

#include "moocbt.h"

#ifdef __cplusplus
extern "C" {
#endif

int moocbt_setup_snapshot(unsigned int minor, char *bdev, char *cow, unsigned long fallocated_space, unsigned long cache_size);

int moocbt_reload_snapshot(unsigned int minor, char *bdev, char *cow, unsigned long cache_size);

int moocbt_reload_incremental(unsigned int minor, char *bdev, char *cow, unsigned long cache_size);

int moocbt_destroy(unsigned int minor);

int moocbt_transition_incremental(unsigned int minor);

int moocbt_transition_snapshot(unsigned int minor, char *cow, unsigned long fallocated_space);

int moocbt_reconfigure(unsigned int minor, unsigned long cache_size);

int moocbt_info(unsigned int minor, struct moocbt_info *info);

int moocbt_expand_cow_file(unsigned int minor, uint64_t size);

int moocbt_reconfigure_auto_expand(unsigned int minor, uint64_t step_size, uint64_t reserved_space);

/**
 * Get the first available minor.
 *
 * @returns non-negative number if minor is available, otherwise -1
 */
int moocbt_get_free_minor(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBMOOCBT_H_ */
