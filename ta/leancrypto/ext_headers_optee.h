/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
/*
 * Stands in for upstream's ext_headers.h - it claims that header's include
 * guard, so the POSIX one is never pulled in. Everything here exists to let
 * the unmodified upstream sources compile against the OP-TEE devkit.
 */
#ifndef EXT_HEADERS_H
#define EXT_HEADERS_H

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <malloc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <tee_internal_api.h>

#define LC_PURE __attribute__((pure))
#define lc_member_size(type, member) (sizeof(((type *)0)->member))
#define LC_DEFINE_CONSTRUCTOR(func) void func(void)
#define LC_DEFINE_DESTRUCTOR(func) void func(void)
#define SYSV_ABI
#define LC_FIPS_RODATA_SECTION
#define noinline __attribute__((noinline))
#ifndef fallthrough
#define fallthrough __attribute__((fallthrough))
#endif

typedef int64_t time64_t;
int lc_get_time(time64_t *seconds, time64_t *nanoseconds);

/*
 * OP-TEE's <time.h> declares only time_t, so the clock interface that
 * architecture_posix.c builds on is declared here. compat.c backs it with
 * TEE_GetSystemTime().
 */
#define CLOCK_REALTIME 0
struct timespec {
	int64_t tv_sec;
	int64_t tv_nsec;
};
int clock_gettime(int clk_id, struct timespec *tp);

/* A TA cannot fork, so the fork detection in seeded_rng.c is a constant. */
typedef int pid_t;
static inline pid_t getpid(void)
{
	return 1;
}

/*
 * memory_support.c is used as-is on top of OP-TEE's allocator. TA memory is
 * managed by OP-TEE, not Linux swap; POSIX page locking is unnecessary.
 */
int posix_memalign(void **ptr, size_t align, size_t size);

/*
 * OP-TEE provides fwrite()/vsnprintf(); binhexbin.c also needs fprintf.
 */
int fprintf(FILE *stream, const char *fmt, ...)
	__attribute__((__format__(__printf__, 2, 3)));

static inline int mlock(const void *addr, size_t len)
{
	(void)addr;
	(void)len;
	return 0;
}

static inline int munlock(const void *addr, size_t len)
{
	(void)addr;
	(void)len;
	return 0;
}

#endif
