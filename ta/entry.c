// SPDX-License-Identifier: BSD-2-Clause
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
#include <tee_internal_api_extensions.h>
#include "commands.h"
#include "crypto.h"

TEE_Result TA_CreateEntryPoint(void)
{
	return pqc_crypto_init();
}

void TA_DestroyEntryPoint(void)
{
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t types, TEE_Param params[4],
				    void **ctx)
{
	TEE_Identity identity;
	struct pqc_session *session;
	TEE_Result result;
	(void)params;
	if (types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE,
				     TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE))
		return TEE_ERROR_BAD_PARAMETERS;
	result = TEE_GetPropertyAsIdentity(TEE_PROPSET_CURRENT_CLIENT,
					   "gpd.client.identity", &identity);
	if (result)
		return result;
	/*
	 * Agreed prototype policy: public Linux clients share one key
	 * namespace.
	 */
	if (identity.login != TEE_LOGIN_PUBLIC)
		return TEE_ERROR_ACCESS_DENIED;
	session = TEE_Malloc(sizeof(*session), TEE_MALLOC_FILL_ZERO);
	if (!session)
		return TEE_ERROR_OUT_OF_MEMORY;
	result = TEE_AllocatePersistentObjectEnumerator(&session->enumerator);
	if (result) {
		TEE_Free(session);
		return result;
	}
	*ctx = session;
	return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void *ctx)
{
	struct pqc_session *session = ctx;

	TEE_FreePersistentObjectEnumerator(session->enumerator);
	TEE_Free(session);
}

TEE_Result TA_InvokeCommandEntryPoint(void *ctx, uint32_t command,
				      uint32_t types, TEE_Param params[4])
{
	uint8_t *input = NULL, *output = NULL;
	size_t length, size;
	TEE_Result result;

	if (types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,
				     TEE_PARAM_TYPE_MEMREF_INPUT,
				     TEE_PARAM_TYPE_MEMREF_OUTPUT,
				     TEE_PARAM_TYPE_NONE) ||
	    params[0].value.a != PQC_VERSION || params[0].value.b)
		return TEE_ERROR_BAD_PARAMETERS;
	length = params[1].memref.size;
	size = params[2].memref.size;
	if (length > PQC_MAX_REQUEST || (!params[1].memref.buffer && length) ||
	    (!params[2].memref.buffer && size))
		return TEE_ERROR_BAD_PARAMETERS;
	if (size > PQC_MAX_RESPONSE)
		size = PQC_MAX_RESPONSE;
	input = TEE_Malloc(length ? length : 1, TEE_MALLOC_FILL_ZERO);
	output = TEE_Malloc(PQC_MAX_RESPONSE, TEE_MALLOC_FILL_ZERO);
	if (!input || !output) {
		result = TEE_ERROR_OUT_OF_MEMORY;
		goto done;
	}
	if (length)
		TEE_MemMove(input, params[1].memref.buffer, length);
	result = pqc_execute(ctx, command, input, length, output, &size);
	if (!result && size)
		TEE_MemMove(params[2].memref.buffer, output, size);
	params[2].memref.size =
		(!result || result == TEE_ERROR_SHORT_BUFFER) ? size : 0;
done:
	pqc_wipe_free(input, length ? length : 1);
	pqc_wipe_free(output, PQC_MAX_RESPONSE);
	return result;
}
