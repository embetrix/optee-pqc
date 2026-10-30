// SPDX-License-Identifier: BSD-2-Clause
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
/* POSIX interfaces needed by the pinned upstream sources, backed by OP-TEE. */
#include <stdarg.h>
#include "ext_headers.h"

int lc_optee_errno;

int posix_memalign(void **ptr, size_t align, size_t size)
{
	void *allocation;
	int saved_errno = errno;

	if (align < sizeof(void *) || (align & (align - 1)))
		return EINVAL;
	if (!size) {
		*ptr = NULL;
		return 0;
	}
	allocation = memalign(align, size);
	/* POSIX returns the error code without changing errno or *ptr on failure. */
	errno = saved_errno;
	if (!allocation)
		return ENOMEM;
	*ptr = allocation;
	return 0;
}

int clock_gettime(int clk_id, struct timespec *tp)
{
	TEE_Time time = { 0 };

	if (!tp || clk_id != CLOCK_REALTIME) {
		errno = EINVAL;
		return -1;
	}
	/* This adapter exposes OP-TEE system time; it makes no UTC epoch promise. */
	TEE_GetSystemTime(&time);
	tp->tv_sec = time.seconds;
	tp->tv_nsec = (int64_t)time.millis * 1000000;
	return 0;
}

int fprintf(FILE *stream, const char *fmt, ...)
{
	char local[256];
	char *buffer = local;
	va_list ap, retry;
	int length, result = -1;

	va_start(ap, fmt);
	va_copy(retry, ap);
	length = vsnprintf(local, sizeof(local), fmt, ap);
	va_end(ap);
	if (length < 0) {
		errno = EINVAL;
		goto out;
	}
	if ((size_t)length >= sizeof(local)) {
		buffer = malloc((size_t)length + 1);
		if (!buffer) {
			errno = ENOMEM;
			goto out;
		}
		if (vsnprintf(buffer, (size_t)length + 1, fmt, retry) !=
		    length) {
			errno = EINVAL;
			goto out;
		}
	}
	if (fwrite(buffer, 1, (size_t)length, stream) != (size_t)length)
		errno = EIO;
	else
		result = length;
out:
	va_end(retry);
	if (buffer != local)
		free(buffer);
	return result;
}
