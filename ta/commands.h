/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
#ifndef PQC_COMMANDS_H
#define PQC_COMMANDS_H
#include <tee_internal_api.h>

struct pqc_session {
	TEE_ObjectEnumHandle enumerator;
	bool listing;
};

/* Input and output are private TA buffers, not shared memory. */
TEE_Result pqc_execute(struct pqc_session *session, uint32_t command,
		       const uint8_t *input, size_t length, uint8_t *output,
		       size_t *size);
#endif
