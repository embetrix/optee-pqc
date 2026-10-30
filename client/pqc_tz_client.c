// SPDX-License-Identifier: BSD-2-Clause
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
#include <stdlib.h>
#include <string.h>
#include "pqc_tz_client.h"

static TEEC_Result local_error(struct pqc_tz_client *client, TEEC_Result result)
{
	if (client)
		client->error_origin = TEEC_ORIGIN_API;
	return result;
}

const char *pqc_result_string(TEEC_Result result)
{
	switch (result) {
	case TEEC_SUCCESS:
		return "Success";
	case TEEC_ERROR_ACCESS_DENIED:
		return "Access denied";
	case TEEC_ERROR_BAD_PARAMETERS:
		return "Invalid parameters";
	case TEEC_ERROR_ITEM_NOT_FOUND:
		return "Item not found";
	case TEEC_ERROR_NOT_SUPPORTED:
		return "Operation not supported";
	case TEEC_ERROR_OUT_OF_MEMORY:
		return "Out of memory";
	case TEEC_ERROR_COMMUNICATION:
		return "Communication with OP-TEE failed";
	case TEEC_ERROR_SHORT_BUFFER:
		return "Output buffer is too small";
	case TEEC_ERROR_TARGET_DEAD:
		return "Trusted application terminated";
	case TEEC_ERROR_SECURITY:
		return "Security check failed";
	case TEE_ERROR_SIGNATURE_INVALID:
		return "Signature verification failed";
	default:
		return "OP-TEE operation failed";
	}
}

const char *pqc_origin_string(uint32_t origin)
{
	switch (origin) {
	case TEEC_ORIGIN_API:
		return "client API";
	case TEEC_ORIGIN_COMMS:
		return "communications";
	case TEEC_ORIGIN_TEE:
		return "TEE";
	case TEEC_ORIGIN_TRUSTED_APP:
		return "TA";
	default:
		return "unknown";
	}
}

void pqc_clear(void *buffer, size_t length)
{
	volatile uint8_t *p = buffer;

	while (length--)
		*p++ = 0;
}

TEEC_Result pqc_open(struct pqc_tz_client *client)
{
	const TEEC_UUID uuid = PQC_TA_UUID;
	TEEC_Result result;

	if (!client)
		return local_error(client, TEEC_ERROR_BAD_PARAMETERS);
	memset(client, 0, sizeof(*client));
	client->error_origin = TEEC_ORIGIN_API;
	result = TEEC_InitializeContext(NULL, &client->context);
	if (result)
		return result;
	result = TEEC_OpenSession(&client->context, &client->session, &uuid,
				  TEEC_LOGIN_PUBLIC, NULL, NULL,
				  &client->error_origin);
	if (result)
		TEEC_FinalizeContext(&client->context);
	else
		client->open = true;
	return result;
}

void pqc_close(struct pqc_tz_client *client)
{
	if (client && client->open) {
		TEEC_CloseSession(&client->session);
		TEEC_FinalizeContext(&client->context);
		client->open = false;
	}
}

TEEC_Result pqc_call(struct pqc_tz_client *client, uint32_t command,
		     const void *request, size_t request_len, void *response,
		     size_t *response_len)
{
	TEEC_SharedMemory in = { .flags = TEEC_MEM_INPUT };
	TEEC_SharedMemory out = { .flags = TEEC_MEM_OUTPUT };
	TEEC_Operation op = { 0 };
	TEEC_Result result;
	size_t capacity;

	if (client)
		client->error_origin = TEEC_ORIGIN_API;
	if (!client || !client->open || !response_len ||
	    request_len > PQC_MAX_REQUEST || (!request && request_len))
		return local_error(client, TEEC_ERROR_BAD_PARAMETERS);
	capacity = *response_len;
	if (capacity > PQC_MAX_RESPONSE || (!response && capacity))
		return local_error(client, TEEC_ERROR_BAD_PARAMETERS);
	in.size = request_len ? request_len : 1;
	out.size = capacity ? capacity : 1;
	result = TEEC_AllocateSharedMemory(&client->context, &in);
	if (result)
		return result;
	result = TEEC_AllocateSharedMemory(&client->context, &out);
	if (result)
		goto done;
	memset(in.buffer, 0, in.size);
	memset(out.buffer, 0, out.size);
	if (request_len)
		memcpy(in.buffer, request, request_len);
	op.paramTypes =
		TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_MEMREF_PARTIAL_INPUT,
				 TEEC_MEMREF_PARTIAL_OUTPUT, TEEC_NONE);
	op.params[0].value.a = PQC_VERSION;
	op.params[1].memref.parent = &in;
	op.params[1].memref.size = request_len;
	op.params[2].memref.parent = &out;
	op.params[2].memref.size = capacity;
	result = TEEC_InvokeCommand(&client->session, command, &op,
				    &client->error_origin);
	if (!result && op.params[2].memref.size > capacity)
		result = local_error(client, TEEC_ERROR_COMMUNICATION);
	if (!result && op.params[2].memref.size)
		memcpy(response, out.buffer, op.params[2].memref.size);
	*response_len = (!result || result == TEEC_ERROR_SHORT_BUFFER)
				? op.params[2].memref.size
				: 0;
	/*
	 * Clear shared-memory secrets immediately after copying, before
	 * release.
	 */
	pqc_clear(out.buffer, out.size);
	TEEC_ReleaseSharedMemory(&out);
done:
	pqc_clear(in.buffer, in.size);
	TEEC_ReleaseSharedMemory(&in);
	return result;
}

TEEC_Result pqc_generate(struct pqc_tz_client *client, uint32_t algorithm,
			 uint8_t *id)
{
	uint8_t request[4];
	size_t length = PQC_ID_SIZE;

	pqc_put32(request, algorithm);
	return pqc_call(client, PQC_GENERATE, request, sizeof(request), id,
			&length);
}

TEEC_Result pqc_delete(struct pqc_tz_client *client, const uint8_t *id)
{
	size_t length = 0;

	return pqc_call(client, PQC_DELETE, id, PQC_ID_SIZE, NULL, &length);
}

TEEC_Result pqc_list(struct pqc_tz_client *client, bool reset, uint8_t *page,
		     size_t *length)
{
	uint8_t request[4];

	pqc_put32(request, reset);
	return pqc_call(client, PQC_LIST, request, sizeof(request), page,
			length);
}

TEEC_Result pqc_public(struct pqc_tz_client *client, const uint8_t *id,
		       uint32_t *algorithm, uint8_t *public_key, size_t *length)
{
	uint8_t response[4 + PQC_MAX_PK_SIZE];
	size_t size = sizeof(response);
	TEEC_Result result;

	if (!length || !algorithm || (!public_key && *length))
		return local_error(client, TEEC_ERROR_BAD_PARAMETERS);
	result = pqc_call(client, PQC_PUBLIC, id, PQC_ID_SIZE, response, &size);
	if (result)
		return result;
	if (size < 4)
		return local_error(client, TEEC_ERROR_COMMUNICATION);
	*algorithm = pqc_get32(response);
	if (!pqc_pk_size(*algorithm) || size != 4 + pqc_pk_size(*algorithm))
		return local_error(client, TEEC_ERROR_COMMUNICATION);
	if (*length < size - 4)
		result = local_error(client, TEEC_ERROR_SHORT_BUFFER);
	else
		memcpy(public_key, response + 4, size - 4);
	*length = size - 4;
	return result;
}

TEEC_Result pqc_encapsulate_ex(struct pqc_tz_client *client, uint32_t algorithm,
			       const uint8_t *public_key, size_t public_len,
			       uint8_t *ciphertext, size_t *ciphertext_len,
			       uint8_t *secret)
{
	uint8_t request[4 + PQC_MAX_PK_SIZE];
	uint8_t response[PQC_MAX_CT_SIZE + PQC_SECRET_SIZE];
	size_t size = pqc_ct_size(algorithm) + PQC_SECRET_SIZE;
	size_t prefix = algorithm == PQC_MLKEM768 ? 0 : 4;
	TEEC_Result result;

	if (!pqc_ct_size(algorithm) || public_len != pqc_pk_size(algorithm) ||
	    !public_key || !ciphertext_len ||
	    (!ciphertext && *ciphertext_len) || !secret)
		return local_error(client, TEEC_ERROR_BAD_PARAMETERS);
	if (*ciphertext_len < pqc_ct_size(algorithm)) {
		*ciphertext_len = pqc_ct_size(algorithm);
		return local_error(client, TEEC_ERROR_SHORT_BUFFER);
	}
	pqc_put32(request, algorithm);
	memcpy(request + prefix, public_key, public_len);
	result =
		pqc_call(client, prefix ? PQC_ENCAPSULATE_ALG : PQC_ENCAPSULATE,
			 request, prefix + public_len, response, &size);
	if (!result && size != pqc_ct_size(algorithm) + PQC_SECRET_SIZE)
		result = local_error(client, TEEC_ERROR_COMMUNICATION);
	if (!result) {
		*ciphertext_len = pqc_ct_size(algorithm);
		memcpy(ciphertext, response, *ciphertext_len);
		memcpy(secret, response + *ciphertext_len, PQC_SECRET_SIZE);
	}
	pqc_clear(response, sizeof(response));
	return result;
}

TEEC_Result pqc_encapsulate(struct pqc_tz_client *client,
			    const uint8_t *public_key, uint8_t *ciphertext,
			    uint8_t *secret)
{
	size_t length = PQC_CT_SIZE;
	return pqc_encapsulate_ex(client, PQC_MLKEM768, public_key,
				  PQC_KEM_PK_SIZE, ciphertext, &length, secret);
}

TEEC_Result pqc_decapsulate_ex(struct pqc_tz_client *client, const uint8_t *id,
			       const uint8_t *ciphertext, size_t ciphertext_len,
			       uint8_t *secret)
{
	uint8_t request[PQC_ID_SIZE + PQC_MAX_CT_SIZE];
	size_t size = PQC_SECRET_SIZE;
	TEEC_Result result;
	bool valid = false;

	for (uint32_t alg = 1; alg <= PQC_ALGORITHM_COUNT; alg++)
		if (pqc_ct_size(alg) && ciphertext_len == pqc_ct_size(alg))
			valid = true;
	if (!id || !ciphertext || !secret || !valid)
		return local_error(client, TEEC_ERROR_BAD_PARAMETERS);
	memcpy(request, id, PQC_ID_SIZE);
	memcpy(request + PQC_ID_SIZE, ciphertext, ciphertext_len);
	result = pqc_call(client, PQC_DECAPSULATE, request,
			  PQC_ID_SIZE + ciphertext_len, secret, &size);
	if (!result && size != PQC_SECRET_SIZE) {
		pqc_clear(secret, PQC_SECRET_SIZE);
		return local_error(client, TEEC_ERROR_COMMUNICATION);
	}
	return result;
}

TEEC_Result pqc_decapsulate(struct pqc_tz_client *client, const uint8_t *id,
			    const uint8_t *ciphertext, uint8_t *secret)
{
	return pqc_decapsulate_ex(client, id, ciphertext, PQC_CT_SIZE, secret);
}

static TEEC_Result dsa(struct pqc_tz_client *client, bool sign, uint32_t algorithm,
		       const uint8_t *key, const uint8_t *message,
		       size_t message_len, const uint8_t *context,
		       size_t context_len, const uint8_t *signature,
		       uint8_t *output, size_t *output_len)
{
	size_t tag = !sign && algorithm != PQC_MLDSA87 ? 4 : 0;
	size_t prefix = sign ? PQC_ID_SIZE
			     : pqc_pk_size(algorithm) + pqc_sig_size(algorithm);
	size_t length = tag + prefix + 8 + context_len + message_len;
	uint8_t *request;
	TEEC_Result result;

	if (!key || (!message && message_len) || (!context && context_len) ||
	    (!sign && !signature) || message_len > PQC_MAX_MESSAGE ||
	    context_len > PQC_MAX_CONTEXT || !output_len ||
	    (*output_len && !output) || *output_len > PQC_MAX_SIG_SIZE)
		return local_error(client, TEEC_ERROR_BAD_PARAMETERS);
	request = malloc(length);
	if (!request)
		return local_error(client, TEEC_ERROR_OUT_OF_MEMORY);
	if (tag)
		pqc_put32(request, algorithm);
	memcpy(request + tag, key, sign ? PQC_ID_SIZE : pqc_pk_size(algorithm));
	if (!sign)
		memcpy(request + tag + pqc_pk_size(algorithm), signature,
		       pqc_sig_size(algorithm));
	prefix += tag;
	pqc_put32(request + prefix, (uint32_t)context_len);
	pqc_put32(request + prefix + 4, (uint32_t)message_len);
	if (context_len)
		memcpy(request + prefix + 8, context, context_len);
	if (message_len)
		memcpy(request + prefix + 8 + context_len, message,
		       message_len);
	result = pqc_call(client,
			  sign	? PQC_SIGN
			  : tag ? PQC_VERIFY_ALG
				: PQC_VERIFY,
			  request, length, output, output_len);
	if (!result) {
		bool valid = !sign && *output_len == 0;
		for (uint32_t alg = 1; sign && alg <= PQC_ALGORITHM_COUNT;
		     alg++)
			if (pqc_sig_size(alg) &&
			    *output_len == pqc_sig_size(alg))
				valid = true;
		if (!valid)
			result = local_error(client, TEEC_ERROR_COMMUNICATION);
	}
	pqc_clear(request, length);
	free(request);
	return result;
}

TEEC_Result pqc_sign_ex(struct pqc_tz_client *client, const uint8_t *id,
			const uint8_t *message, size_t message_len,
			const uint8_t *context, size_t context_len,
			uint8_t *signature, size_t *signature_len)
{
	return dsa(client, true, 0, id, message, message_len, context,
		   context_len, NULL, signature, signature_len);
}

TEEC_Result pqc_sign(struct pqc_tz_client *client, const uint8_t *id,
		     const uint8_t *message, size_t message_len,
		     const uint8_t *context, size_t context_len,
		     uint8_t *signature)
{
	size_t size = PQC_SIG_SIZE;
	TEEC_Result result =
		pqc_sign_ex(client, id, message, message_len, context,
			    context_len, signature, &size);
	if (!result && size != PQC_SIG_SIZE)
		return local_error(client, TEEC_ERROR_BAD_PARAMETERS);
	return result;
}

TEEC_Result pqc_verify_ex(struct pqc_tz_client *client, uint32_t algorithm,
			  const uint8_t *public_key, size_t public_len,
			  const uint8_t *message, size_t message_len,
			  const uint8_t *context, size_t context_len,
			  const uint8_t *signature, size_t signature_len)
{
	size_t size = 0;
	if (!pqc_sig_size(algorithm) || public_len != pqc_pk_size(algorithm) ||
	    signature_len != pqc_sig_size(algorithm))
		return local_error(client, TEEC_ERROR_BAD_PARAMETERS);
	return dsa(client, false, algorithm, public_key, message, message_len,
		   context, context_len, signature, NULL, &size);
}

TEEC_Result pqc_verify(struct pqc_tz_client *client, const uint8_t *public_key,
		       const uint8_t *message, size_t message_len,
		       const uint8_t *context, size_t context_len,
		       const uint8_t *signature)
{
	return pqc_verify_ex(client, PQC_MLDSA87, public_key, PQC_DSA_PK_SIZE,
			     message, message_len, context, context_len,
			     signature, PQC_SIG_SIZE);
}
