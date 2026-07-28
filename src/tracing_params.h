// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2022 Datto Inc.
 * Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
 */

#ifndef TRACING_PARAMS_H_
#define TRACING_PARAMS_H_

#include "includes.h"

#ifdef USE_HOOK_TRACER
#include <linux/completion.h>
#endif //USE_HOOK_TRACER

struct bio;
struct snap_device;

struct bsector_list {
        struct bio_sector_map *head;
        struct bio_sector_map *tail;
};

struct tracing_params {
        struct bio *orig_bio;
        struct snap_device *dev;
        atomic_t refs;
        struct bsector_list bio_sects;
#ifdef USE_HOOK_TRACER
	// fired once all read clones have read the original data into memory
	struct completion read_done;
	// counts outstanding clones plus one additional reference
	atomic_t pending_reads;
#endif //USE_HOOK_TRACER
};

int tp_alloc(struct snap_device *dev, struct bio *bio,
             struct tracing_params **tp_out);
void tp_get(struct tracing_params *tp);
void tp_put(struct tracing_params *tp);
int tp_add(struct tracing_params *tp, struct bio *bio);

#endif /* TRACING_PARAMS_H_ */
