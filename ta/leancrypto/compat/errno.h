/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
/*
 * OP-TEE has no <errno.h>. leancrypto carries the constants in
 * errno_private-base.h, so only the errno variable itself is missing.
 */
#ifndef PQC_COMPAT_ERRNO_H
#define PQC_COMPAT_ERRNO_H

#include "errno_private.h"

extern int lc_optee_errno;
#define errno lc_optee_errno

#endif
