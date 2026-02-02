// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2025 Datto Inc.
 * Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
 */

#ifndef MEMORY_H_
#define MEMORY_H_

#include "includes.h"

unsigned long moocbt_get_unmapped_area(struct file *file, unsigned long addr, unsigned long len, unsigned long pgoff, unsigned long flags);

#endif /* MEMORY_H_ */
 