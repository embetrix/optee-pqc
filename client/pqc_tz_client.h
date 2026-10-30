/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
#ifndef PQC_TZ_CLIENT_H
#define PQC_TZ_CLIENT_H
#include <stdbool.h>
#include <tee_client_api.h>
#include "pqc_algorithms.h"
struct pqc_tz_client {
	TEEC_Context context;
	TEEC_Session session;
	bool open;
	/* Origin of the most recent failing operation; local checks use API. */
	uint32_t error_origin;
};
const char *pqc_result_string(TEEC_Result result);
const char *pqc_origin_string(uint32_t origin);
void pqc_clear(void *buffer, size_t length);
TEEC_Result pqc_open(struct pqc_tz_client *client);
void pqc_close(struct pqc_tz_client *client);
TEEC_Result pqc_call(struct pqc_tz_client *client, uint32_t command,
		     const void *request, size_t request_len, void *response,
		     size_t *response_len);
TEEC_Result pqc_generate(struct pqc_tz_client *client, uint32_t algorithm,
			 uint8_t *id);
TEEC_Result pqc_delete(struct pqc_tz_client *client, const uint8_t *id);
TEEC_Result pqc_public(struct pqc_tz_client *client, const uint8_t *id,
		       uint32_t *algorithm, uint8_t *public_key,
		       size_t *length);
TEEC_Result pqc_list(struct pqc_tz_client *client, bool reset, uint8_t *page,
		     size_t *length);
TEEC_Result pqc_encapsulate(struct pqc_tz_client *client,
			    const uint8_t *public_key, uint8_t *ciphertext,
			    uint8_t *secret);
TEEC_Result pqc_decapsulate(struct pqc_tz_client *client, const uint8_t *id,
			    const uint8_t *ciphertext, uint8_t *secret);
TEEC_Result pqc_sign(struct pqc_tz_client *client, const uint8_t *id,
		     const uint8_t *message, size_t message_len,
		     const uint8_t *context, size_t context_len,
		     uint8_t *signature);
TEEC_Result pqc_verify(struct pqc_tz_client *client, const uint8_t *public_key,
		       const uint8_t *message, size_t message_len,
		       const uint8_t *context, size_t context_len,
		       const uint8_t *signature);
/* All parameter sets: capacities on entry, actual/required lengths on return. */
TEEC_Result pqc_encapsulate_ex(struct pqc_tz_client *client, uint32_t algorithm,
			       const uint8_t *public_key, size_t public_len,
			       uint8_t *ciphertext, size_t *ciphertext_len,
			       uint8_t *secret);
TEEC_Result pqc_decapsulate_ex(struct pqc_tz_client *client, const uint8_t *id,
			       const uint8_t *ciphertext, size_t ciphertext_len,
			       uint8_t *secret);
TEEC_Result pqc_sign_ex(struct pqc_tz_client *client, const uint8_t *id,
			const uint8_t *message, size_t message_len,
			const uint8_t *context, size_t context_len,
			uint8_t *signature, size_t *signature_len);
TEEC_Result pqc_verify_ex(struct pqc_tz_client *client, uint32_t algorithm,
			  const uint8_t *public_key, size_t public_len,
			  const uint8_t *message, size_t message_len,
			  const uint8_t *context, size_t context_len,
			  const uint8_t *signature, size_t signature_len);
#endif
