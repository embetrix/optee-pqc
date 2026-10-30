// SPDX-License-Identifier: BSD-2-Clause
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
#include <tee_internal_api_extensions.h>
#include "storage.h"
#include "crypto.h"

/* Explicit REE storage; this prototype does not promise rollback protection. */
#define STORAGE TEE_STORAGE_PRIVATE_REE
#define RECORD_MAGIC 0x31435150u /* PQC1 */
#define POLICY_TRUSTED_CLIENT 1u
#define OBJECT_ID_SIZE (4u + PQC_ID_SIZE)
#define PQC_RECORD_HEADER 24u
#define PQC_RECORD_MAX (PQC_RECORD_HEADER + PQC_MAX_PK_SIZE + PQC_MAX_SK_SIZE)

struct pqc_key {
	uint32_t algorithm;
	uint8_t record[PQC_RECORD_MAX];
};

uint32_t pqc_key_algorithm(const struct pqc_key *key)
{
	return key->algorithm;
}

const uint8_t *pqc_key_public(const struct pqc_key *key)
{
	return key->record + PQC_RECORD_HEADER;
}

const uint8_t *pqc_key_private(const struct pqc_key *key)
{
	return pqc_key_public(key) + pqc_pk_size(key->algorithm);
}

void pqc_key_free(struct pqc_key *key)
{
	pqc_wipe_free(key, sizeof(*key));
}

static void object_id(uint8_t *object, const uint8_t *id)
{
	TEE_MemMove(object, "pqc1", 4);
	TEE_MemMove(object + 4, id, PQC_ID_SIZE);
}

TEE_Result pqc_store_generate(uint32_t algorithm, uint8_t id[PQC_ID_SIZE])
{
	uint8_t *record = TEE_Malloc(PQC_RECORD_MAX, TEE_MALLOC_FILL_ZERO);
	uint8_t object[OBJECT_ID_SIZE];
	TEE_ObjectHandle handle = TEE_HANDLE_NULL;
	size_t pk_len = pqc_pk_size(algorithm), sk_len = pqc_sk_size(algorithm);
	TEE_Result result = TEE_ERROR_BAD_PARAMETERS;

	if (!record)
		return TEE_ERROR_OUT_OF_MEMORY;
	if (!pk_len || !sk_len)
		goto out;
	pqc_put32(record, RECORD_MAGIC);
	pqc_put32(record + 4, PQC_VERSION);
	pqc_put32(record + 8, algorithm);
	pqc_put32(record + 12, (uint32_t)pk_len);
	pqc_put32(record + 16, (uint32_t)sk_len);
	pqc_put32(record + 20, POLICY_TRUSTED_CLIENT);
	result = pqc_crypto_generate(algorithm, record + PQC_RECORD_HEADER,
				     record + PQC_RECORD_HEADER + pk_len);
	if (result)
		goto out;
	for (unsigned int attempt = 0; attempt < 4; attempt++) {
		TEE_GenerateRandom(id, PQC_ID_SIZE);
		object_id(object, id);
		/* No OVERWRITE: a random ID collision must never replace a key.
		 */
		result = TEE_CreatePersistentObject(
			STORAGE, object, sizeof(object),
			TEE_DATA_FLAG_ACCESS_READ, TEE_HANDLE_NULL, record,
			PQC_RECORD_HEADER + pk_len + sk_len, &handle);
		if (result != TEE_ERROR_ACCESS_CONFLICT)
			break;
	}
	if (!result)
		TEE_CloseObject(handle);
out:
	pqc_wipe_free(record, PQC_RECORD_MAX);
	return result;
}

static TEE_Result read_exact(TEE_ObjectHandle handle, void *out, size_t size)
{
	size_t count = 0;
	TEE_Result result = TEE_ReadObjectData(handle, out, size, &count);

	if (result)
		return result;
	return count == size ? TEE_SUCCESS : TEE_ERROR_CORRUPT_OBJECT;
}

/* Validate the entire record shape before reading any key bytes. The open
 * read handle excludes writers/deletion until the caller closes it. */
static TEE_Result open_record(const uint8_t *id, TEE_ObjectHandle *handle,
			      uint8_t header[PQC_RECORD_HEADER],
			      uint32_t *algorithm)
{
	uint8_t object[OBJECT_ID_SIZE];
	TEE_ObjectInfo info;
	TEE_Result result;

	*handle = TEE_HANDLE_NULL;
	object_id(object, id);
	result = TEE_OpenPersistentObject(
		STORAGE, object, sizeof(object),
		TEE_DATA_FLAG_ACCESS_READ | TEE_DATA_FLAG_SHARE_READ, handle);
	if (result)
		return result;
	result = TEE_GetObjectInfo1(*handle, &info);
	if (result)
		goto fail;
	if (info.dataSize < PQC_RECORD_HEADER ||
	    info.dataSize > PQC_RECORD_MAX) {
		result = TEE_ERROR_CORRUPT_OBJECT;
		goto fail;
	}
	result = read_exact(*handle, header, PQC_RECORD_HEADER);
	if (result)
		goto fail;
	*algorithm = pqc_get32(header + 8);
	if (pqc_get32(header) != RECORD_MAGIC ||
	    pqc_get32(header + 4) != PQC_VERSION || !pqc_pk_size(*algorithm) ||
	    pqc_get32(header + 12) != pqc_pk_size(*algorithm) ||
	    pqc_get32(header + 16) != pqc_sk_size(*algorithm) ||
	    pqc_get32(header + 20) != POLICY_TRUSTED_CLIENT ||
	    info.dataSize != PQC_RECORD_HEADER + pqc_pk_size(*algorithm) +
				     pqc_sk_size(*algorithm)) {
		result = TEE_ERROR_CORRUPT_OBJECT;
		goto fail;
	}
	return TEE_SUCCESS;
fail:
	TEE_CloseObject(*handle);
	*handle = TEE_HANDLE_NULL;
	return result;
}

static TEE_Result load_metadata(const uint8_t *id, uint32_t *algorithm)
{
	uint8_t header[PQC_RECORD_HEADER];
	TEE_ObjectHandle handle;
	TEE_Result result = open_record(id, &handle, header, algorithm);

	if (!result)
		TEE_CloseObject(handle);
	return result;
}

TEE_Result pqc_store_public(const uint8_t *id, uint32_t *algorithm,
			    uint8_t *out, size_t *size)
{
	uint8_t header[PQC_RECORD_HEADER];
	TEE_ObjectHandle handle;
	size_t capacity = *size;
	TEE_Result result = open_record(id, &handle, header, algorithm);

	if (result)
		return result;
	*size = pqc_pk_size(*algorithm);
	result = capacity < *size ? TEE_ERROR_SHORT_BUFFER
				  : read_exact(handle, out, *size);
	TEE_CloseObject(handle);
	return result;
}

TEE_Result pqc_store_load(const uint8_t *id, struct pqc_key **key)
{
	uint8_t header[PQC_RECORD_HEADER];
	uint32_t algorithm;
	struct pqc_key *loaded;
	TEE_ObjectHandle handle;
	TEE_Result result;

	*key = NULL;
	result = open_record(id, &handle, header, &algorithm);
	if (result)
		return result;
	loaded = TEE_Malloc(sizeof(*loaded), TEE_MALLOC_FILL_ZERO);
	if (!loaded) {
		result = TEE_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	loaded->algorithm = algorithm;
	TEE_MemMove(loaded->record, header, sizeof(header));
	result = read_exact(handle, loaded->record + PQC_RECORD_HEADER,
			    pqc_pk_size(algorithm) + pqc_sk_size(algorithm));
	if (!result) {
		*key = loaded;
		loaded = NULL;
	}
out:
	TEE_CloseObject(handle);
	pqc_key_free(loaded);
	return result;
}

TEE_Result pqc_store_delete(const uint8_t *id)
{
	uint8_t object[OBJECT_ID_SIZE];
	TEE_ObjectHandle handle = TEE_HANDLE_NULL;
	TEE_Result result;

	object_id(object, id);
	result = TEE_OpenPersistentObject(STORAGE, object, sizeof(object),
					  TEE_DATA_FLAG_ACCESS_WRITE_META,
					  &handle);
	if (result)
		return result;
	return TEE_CloseAndDeletePersistentObject1(handle);
}

TEE_Result pqc_store_list_start(TEE_ObjectEnumHandle enumerator)
{
	return TEE_StartPersistentObjectEnumerator(enumerator, STORAGE);
}

TEE_Result pqc_store_list(TEE_ObjectEnumHandle enumerator, uint8_t *out,
			  size_t *size)
{
	uint8_t object[TEE_OBJECT_ID_MAX_LEN];
	TEE_ObjectInfo info;
	TEE_Result result;
	*size = 0;
	while (*size < PQC_LIST_PAGE_SIZE) {
		size_t length = sizeof(object);
		uint32_t algorithm;

		result = TEE_GetNextPersistentObject(enumerator, &info, object,
						     &length);
		if (result == TEE_ERROR_ITEM_NOT_FOUND)
			return TEE_SUCCESS;
		if (result)
			return result;
		if (length != OBJECT_ID_SIZE ||
		    TEE_MemCompare(object, "pqc1", 4))
			continue;
		result = load_metadata(object + 4, &algorithm);
		/* A key may have been deleted between successive pages. */
		if (result == TEE_ERROR_ITEM_NOT_FOUND)
			continue;
		if (result)
			return result;
		TEE_MemMove(out + *size, object + 4, PQC_ID_SIZE);
		pqc_put32(out + *size + PQC_ID_SIZE, algorithm);
		*size += PQC_LIST_ENTRY_SIZE;
	}
	return TEE_SUCCESS;
}
