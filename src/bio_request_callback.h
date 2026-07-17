// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022 Datto Inc.
// Additional contributions by Slide are Copyright (C) 2026 Project Orca Inc.

/**
 *
 * DOC: bio_request_callback header.
 *
 * Defines the various mechanisms and types needed to submit IO requests.
 *
 * Different types and callbacks are used between different versions of
 * Linux in order to submit in-flight IO to the kernel.
 *
 * The purpose of this header is to provide a unified interface to making this
 * happen.
 */

#ifndef BIO_REQUEST_CALLBACK_H_INCLUDE
#define BIO_REQUEST_CALLBACK_H_INCLUDE

#include "bio_helper.h" // needed for USE_BDOPS_SUBMIT_BIO to be defined
#include "includes.h"
#include "mrf.h"

#ifdef USE_BDOPS_SUBMIT_BIO

/**
 * submit_bio_fn() - Prototype for the submit_bio function, which will be our
 * hook to intercept IO on kernels >= 5.9
 */
typedef MRF_RETURN_TYPE (submit_bio_fn) (struct bio *bio);

#define BIO_REQUEST_CALLBACK_FN submit_bio_fn
#define SUBMIT_BIO_REAL moocbt_call_mrf_real
#else
#define BIO_REQUEST_CALLBACK_FN make_request_fn
#define SUBMIT_BIO_REAL moocbt_call_mrf_real
#define GET_BIO_REQUEST_TRACKING_PTR moocbt_get_bd_mrf
#define GET_BIO_REQUEST_TRACKING_PTR_GD moocbt_get_gd_mrf
#endif

#endif
