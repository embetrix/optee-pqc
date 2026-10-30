/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
#ifndef PQC_STORAGE_H
#define PQC_STORAGE_H
#include <tee_internal_api.h>
#include "pqc_protocol.h"
/* Owns private key material. Views remain valid until pqc_key_free(). */
struct pqc_key;
uint32_t pqc_key_algorithm(const struct pqc_key *key);
const uint8_t *pqc_key_public(const struct pqc_key *key);
const uint8_t *pqc_key_private(const struct pqc_key *key);
void pqc_key_free(struct pqc_key *key);
TEE_Result pqc_store_generate(uint32_t algorithm, uint8_t id[PQC_ID_SIZE]);
TEE_Result pqc_store_load(const uint8_t *id, struct pqc_key **key);
/* Reads only the validated header and public bytes; never loads the private key. */
TEE_Result pqc_store_public(const uint8_t *id, uint32_t *algorithm,
			    uint8_t *out, size_t *size);
TEE_Result pqc_store_delete(const uint8_t *id);
TEE_Result pqc_store_list_start(TEE_ObjectEnumHandle enumerator);
TEE_Result pqc_store_list(TEE_ObjectEnumHandle enumerator, uint8_t *out,
			  size_t *size);
#endif
