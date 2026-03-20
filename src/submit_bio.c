// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2022 Datto Inc.
 * Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.
 */

#include "submit_bio.h"

#include "bio_helper.h" // needed for USE_BDOPS_SUBMIT_BIO to be defined
#include "callback_refs.h"
#include "includes.h"
#include "logging.h"
#include "paging_helper.h"
#include "snap_device.h"

#ifdef USE_BDOPS_SUBMIT_BIO

#ifdef USE_FENTRY_OFFSET
/*
 * For ftrace to work, each function has a preamble that calls a "function" (asm
 * snippet) called __fentry__ which then triggers the callbacks. If we want to
 * recurse without triggering ftrace, we'll need to skip this preamble. Don't
 * worry, the stack pointer manipulation is right after the call.
 */
MRF_RETURN_TYPE (*moocbt_submit_bio_noacct_passthrough)(struct bio *) =
	(MRF_RETURN_TYPE(*)(struct bio *))((unsigned long)(submit_bio_noacct) +
        FENTRY_CALL_INSTR_BYTES);
#else
typeof(submit_bio_noacct) *moocbt_submit_bio_noacct_passthrough = submit_bio_noacct;
#endif

MRF_RETURN_TYPE moocbt_submit_bio_real(
    struct snap_device* dev,
    struct bio *bio)
{
    #ifdef HAVE_MAKE_REQUEST_FN_INT
    int ret = moocbt_submit_bio_noacct_passthrough(bio);
    MRF_RETURN(ret);
    #else
    moocbt_submit_bio_noacct_passthrough(bio);
    MRF_RETURN(0);
    #endif
}

#endif
