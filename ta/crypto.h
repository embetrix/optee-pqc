/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
#ifndef PQC_CRYPTO_H
#define PQC_CRYPTO_H
#include <tee_internal_api.h>
#include "pqc_algorithms.h"
TEE_Result pqc_crypto_init(void);
TEE_Result pqc_crypto_generate(uint32_t algorithm, uint8_t *pk, uint8_t *sk);
TEE_Result pqc_crypto_encapsulate(uint32_t algorithm, const uint8_t *public_key,
				  uint8_t *ciphertext, uint8_t *secret);
TEE_Result pqc_crypto_decapsulate(uint32_t algorithm,
				  const uint8_t *private_key,
				  const uint8_t *ciphertext, uint8_t *secret);
TEE_Result pqc_crypto_sign(uint32_t algorithm, const uint8_t *private_key,
			   const uint8_t *context, size_t context_len,
			   const uint8_t *message, size_t message_len,
			   uint8_t *signature);
TEE_Result pqc_crypto_verify(uint32_t algorithm, const uint8_t *public_key,
			     const uint8_t *signature, const uint8_t *context,
			     size_t context_len, const uint8_t *message,
			     size_t message_len);
void pqc_wipe_free(void *buffer, size_t size);
#endif
