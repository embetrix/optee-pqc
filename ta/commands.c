// SPDX-License-Identifier: BSD-2-Clause
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
#include <tee_internal_api_extensions.h>
#include "commands.h"
#include "crypto.h"
#include "storage.h"

static TEE_Result output_size(size_t *size, size_t needed)
{
	size_t capacity = *size;

	*size = needed;
	return capacity < needed ? TEE_ERROR_SHORT_BUFFER : TEE_SUCCESS;
}

static TEE_Result handle_generate(const uint8_t *in, size_t length,
				  uint8_t *out, size_t *size)
{
	TEE_Result result;

	if (length != 4 || !pqc_pk_size(pqc_get32(in)))
		return TEE_ERROR_BAD_PARAMETERS;
	/* Check capacity before generating or persisting a key. */
	result = output_size(size, PQC_ID_SIZE);
	if (result)
		return result;
	return pqc_store_generate(pqc_get32(in), out);
}

static TEE_Result handle_delete(const uint8_t *in, size_t length, size_t *size)
{
	if (length != PQC_ID_SIZE)
		return TEE_ERROR_BAD_PARAMETERS;
	*size = 0;
	return pqc_store_delete(in);
}

static TEE_Result handle_public(const uint8_t *in, size_t length, uint8_t *out,
				size_t *size)
{
	uint32_t algorithm;
	size_t public_size = *size >= 4 ? *size - 4 : 0;
	TEE_Result result;

	if (length != PQC_ID_SIZE)
		return TEE_ERROR_BAD_PARAMETERS;
	result = pqc_store_public(in, &algorithm, *size >= 4 ? out + 4 : NULL,
				  &public_size);
	if (!result || result == TEE_ERROR_SHORT_BUFFER)
		*size = 4 + public_size;
	if (!result)
		pqc_put32(out, algorithm);
	return result;
}

static TEE_Result handle_list(struct pqc_session *session, const uint8_t *in,
			      size_t length, uint8_t *out, size_t *size)
{
	TEE_Result result;

	if (length != 4 || pqc_get32(in) > 1)
		return TEE_ERROR_BAD_PARAMETERS;
	/* A short buffer must neither reset nor advance the enumerator. */
	result = output_size(size, PQC_LIST_PAGE_SIZE);
	if (result)
		return result;
	if (pqc_get32(in)) {
		TEE_ResetPersistentObjectEnumerator(session->enumerator);
		session->listing = false;
		result = pqc_store_list_start(session->enumerator);
		if (result == TEE_ERROR_ITEM_NOT_FOUND) {
			*size = 0;
			return TEE_SUCCESS;
		}
		if (result)
			return result;
		session->listing = true;
	}
	if (!session->listing) {
		*size = 0;
		return TEE_SUCCESS;
	}
	result = pqc_store_list(session->enumerator, out, size);
	if (result || *size < PQC_LIST_PAGE_SIZE) {
		TEE_ResetPersistentObjectEnumerator(session->enumerator);
		session->listing = false;
	}
	return result;
}

static TEE_Result handle_encapsulate(uint32_t algorithm, const uint8_t *in,
				     size_t length, uint8_t *out, size_t *size)
{
	TEE_Result result;

	if (!pqc_ct_size(algorithm) || length != pqc_pk_size(algorithm))
		return TEE_ERROR_BAD_PARAMETERS;
	result = output_size(size, pqc_ct_size(algorithm) + PQC_SECRET_SIZE);
	if (result)
		return result;
	return pqc_crypto_encapsulate(algorithm, in, out,
				      out + pqc_ct_size(algorithm));
}

static TEE_Result handle_decapsulate(const uint8_t *in, size_t length,
				     uint8_t *out, size_t *size)
{
	struct pqc_key *key = NULL;
	TEE_Result result;

	if (length < PQC_ID_SIZE)
		return TEE_ERROR_BAD_PARAMETERS;
	result = pqc_store_load(in, &key);
	if (result)
		return result;
	if (!pqc_ct_size(pqc_key_algorithm(key)) ||
	    length != PQC_ID_SIZE + pqc_ct_size(pqc_key_algorithm(key))) {
		pqc_key_free(key);
		return TEE_ERROR_BAD_PARAMETERS;
	}
	result = output_size(size, PQC_SECRET_SIZE);
	if (!result)
		result = pqc_crypto_decapsulate(pqc_key_algorithm(key),
						pqc_key_private(key),
						in + PQC_ID_SIZE, out);
	pqc_key_free(key);
	return result;
}

struct dsa_input {
	const uint8_t *context;
	const uint8_t *message;
	size_t context_len;
	size_t message_len;
};

static TEE_Result parse_dsa(const uint8_t *in, size_t length, size_t prefix,
			    struct dsa_input *parsed)
{
	if (length < prefix + 8)
		return TEE_ERROR_BAD_PARAMETERS;
	parsed->context_len = pqc_get32(in + prefix);
	parsed->message_len = pqc_get32(in + prefix + 4);
	if (parsed->context_len > PQC_MAX_CONTEXT ||
	    parsed->message_len > PQC_MAX_MESSAGE ||
	    length != prefix + 8 + parsed->context_len + parsed->message_len)
		return TEE_ERROR_BAD_PARAMETERS;
	parsed->context = in + prefix + 8;
	parsed->message = parsed->context + parsed->context_len;
	return TEE_SUCCESS;
}

static TEE_Result handle_sign(const uint8_t *in, size_t length, uint8_t *out,
			      size_t *size)
{
	struct dsa_input parsed;
	struct pqc_key *key = NULL;
	TEE_Result result = parse_dsa(in, length, PQC_ID_SIZE, &parsed);

	if (result)
		return result;
	result = pqc_store_load(in, &key);
	if (result)
		return result;
	if (!pqc_sig_size(pqc_key_algorithm(key))) {
		pqc_key_free(key);
		return TEE_ERROR_BAD_PARAMETERS;
	}
	result = output_size(size, pqc_sig_size(pqc_key_algorithm(key)));
	if (!result)
		result = pqc_crypto_sign(pqc_key_algorithm(key),
					 pqc_key_private(key), parsed.context,
					 parsed.context_len, parsed.message,
					 parsed.message_len, out);
	pqc_key_free(key);
	return result;
}

static TEE_Result handle_verify(uint32_t algorithm, const uint8_t *in,
				size_t length, size_t *size)
{
	struct dsa_input parsed;
	TEE_Result result;

	if (!pqc_sig_size(algorithm))
		return TEE_ERROR_BAD_PARAMETERS;
	result = parse_dsa(in, length,
			   pqc_pk_size(algorithm) + pqc_sig_size(algorithm),
			   &parsed);

	if (result)
		return result;
	*size = 0;
	return pqc_crypto_verify(algorithm, in, in + pqc_pk_size(algorithm),
				 parsed.context, parsed.context_len,
				 parsed.message, parsed.message_len);
}

TEE_Result pqc_execute(struct pqc_session *session, uint32_t command,
		       const uint8_t *input, size_t length, uint8_t *output,
		       size_t *size)
{
	switch (command) {
	case PQC_GENERATE:
		return handle_generate(input, length, output, size);
	case PQC_LIST:
		return handle_list(session, input, length, output, size);
	case PQC_PUBLIC:
		return handle_public(input, length, output, size);
	case PQC_DELETE:
		return handle_delete(input, length, size);
	case PQC_ENCAPSULATE:
		return handle_encapsulate(PQC_MLKEM768, input, length, output,
					  size);
	case PQC_DECAPSULATE:
		return handle_decapsulate(input, length, output, size);
	case PQC_SIGN:
		return handle_sign(input, length, output, size);
	case PQC_VERIFY:
		return handle_verify(PQC_MLDSA87, input, length, size);
	case PQC_ENCAPSULATE_ALG:
		if (length < 4)
			return TEE_ERROR_BAD_PARAMETERS;
		return handle_encapsulate(pqc_get32(input), input + 4,
					  length - 4, output, size);
	case PQC_VERIFY_ALG:
		if (length < 4)
			return TEE_ERROR_BAD_PARAMETERS;
		return handle_verify(pqc_get32(input), input + 4, length - 4,
				     size);
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}
}
