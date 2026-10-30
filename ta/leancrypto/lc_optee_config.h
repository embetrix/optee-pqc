/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
/*
 * Force-included into every leancrypto translation unit. Algorithm selection
 * lives in meson-options.txt and is applied by meson setup; this header only
 * carries what the OP-TEE environment itself requires.
 */
#ifndef PQC_LEANCRYPTO_CONFIG_H
#define PQC_LEANCRYPTO_CONFIG_H

#define LC_OPTEE

/* Preinclude the OP-TEE platform header before upstream's ext_headers.h. */
#include "ext_headers_optee.h"
#include "visibility.h"

/* TA_CreateEntryPoint calls lc_init(); no ELF constructors are needed. */
#undef LC_CONSTRUCTOR
#define LC_CONSTRUCTOR(func, priority) \
	void func(void);               \
	void func(void)

#endif
