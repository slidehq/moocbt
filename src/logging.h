// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2022 Datto Inc.
 * Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
 */

#ifndef LOGGING_H_
#define LOGGING_H_

#include "includes.h"

#define MOOCBT_TAG "moocbt"

// printing macros
#define LOG_DEBUG(fmt, args...)                                                \
        do {                                                                   \
                if (moocbt_debug)                                             \
                        printk(KERN_DEBUG MOOCBT_TAG ": " fmt "\n", ##args);    \
        } while (0)

#define LOG_WARN(fmt, args...) printk(KERN_WARNING MOOCBT_TAG ": " fmt "\n", ##args)
#define LOG_ERROR(error, fmt, args...)                                         \
        printk(KERN_ERR MOOCBT_TAG ": " fmt ": %d\n", ##args, error)
#define PRINT_BIO(text, bio)                                                   \
        LOG_DEBUG(text ": sect = %llu size = %u",                              \
                  (unsigned long long)bio_sector(bio), bio_size(bio) / 512)

extern int moocbt_debug;

#endif
