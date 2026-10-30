// SPDX-License-Identifier: BSD-2-Clause
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
#include "crypto.h"
#include "lc_dilithium.h"
#include "lc_kyber.h"
#include "lc_init.h"
#include "lc_memset_secure.h"

static TEE_Result convert(int error)
{
	switch (error) {
	case 0:
		return TEE_SUCCESS;
	case -ENOMEM:
		return TEE_ERROR_OUT_OF_MEMORY;
	case -EBADMSG:
		return TEE_ERROR_SIGNATURE_INVALID;
	case -EINVAL:
		return TEE_ERROR_BAD_PARAMETERS;
	default:
		return TEE_ERROR_SECURITY;
	}
}

void pqc_wipe_free(void *buffer, size_t size)
{
	if (buffer) {
		lc_memset_secure(buffer, 0, size);
		TEE_Free(buffer);
	}
}

static enum lc_kyber_type kem_type(uint32_t algorithm)
{
	switch (algorithm) {
	case PQC_MLKEM512:
		return LC_KYBER_512;
	case PQC_MLKEM768:
		return LC_KYBER_768;
	case PQC_MLKEM1024:
		return LC_KYBER_1024;
	default:
		return LC_KYBER_UNKNOWN;
	}
}

static enum lc_dilithium_type dsa_type(uint32_t algorithm)
{
	switch (algorithm) {
	case PQC_MLDSA44:
		return LC_DILITHIUM_44;
	case PQC_MLDSA65:
		return LC_DILITHIUM_65;
	case PQC_MLDSA87:
		return LC_DILITHIUM_87;
	default:
		return LC_DILITHIUM_UNKNOWN;
	}
}

TEE_Result pqc_crypto_init(void)
{
	return convert(lc_init(0));
}

TEE_Result pqc_crypto_generate(uint32_t algorithm, uint8_t *pk, uint8_t *sk)
{
	union keys {
		struct {
			struct lc_kyber_pk pk;
			struct lc_kyber_sk sk;
		} kem;
		struct {
			struct lc_dilithium_pk pk;
			struct lc_dilithium_sk sk;
		} dsa;
	} *keys = TEE_Malloc(sizeof(*keys), TEE_MALLOC_FILL_ZERO);
	uint8_t *public_bytes = NULL, *private_bytes = NULL;
	size_t public_len = 0, private_len = 0;
	int error = -EINVAL;

	if (!keys)
		return TEE_ERROR_OUT_OF_MEMORY;
	if (pqc_ct_size(algorithm)) {
		error = lc_kyber_keypair(&keys->kem.pk, &keys->kem.sk,
					 lc_seeded_rng, kem_type(algorithm));
		if (error)
			goto out;
		error = lc_kyber_pk_ptr(&public_bytes, &public_len,
					&keys->kem.pk);
		if (error)
			goto out;
		error = lc_kyber_sk_ptr(&private_bytes, &private_len,
					&keys->kem.sk);
		if (error)
			goto out;
	} else if (pqc_sig_size(algorithm)) {
		error = lc_dilithium_keypair(&keys->dsa.pk, &keys->dsa.sk,
					     lc_seeded_rng,
					     dsa_type(algorithm));
		if (error)
			goto out;
		error = lc_dilithium_pk_ptr(&public_bytes, &public_len,
					    &keys->dsa.pk);
		if (error)
			goto out;
		error = lc_dilithium_sk_ptr(&private_bytes, &private_len,
					    &keys->dsa.sk);
		if (error)
			goto out;
	} else {
		goto out;
	}
	if (public_len != pqc_pk_size(algorithm) ||
	    private_len != pqc_sk_size(algorithm)) {
		error = -EFAULT;
		goto out;
	}
	TEE_MemMove(pk, public_bytes, public_len);
	TEE_MemMove(sk, private_bytes, private_len);
out:
	pqc_wipe_free(keys, sizeof(*keys));
	return convert(error);
}

static int export_secret(struct lc_kyber_ss *secret, uint8_t *output)
{
	uint8_t *bytes;
	size_t length;
	int error = lc_kyber_ss_ptr(&bytes, &length, secret);

	if (error)
		return error;
	if (length != PQC_SECRET_SIZE)
		return -EFAULT;
	TEE_MemMove(output, bytes, length);
	return 0;
}

TEE_Result pqc_crypto_encapsulate(uint32_t algorithm, const uint8_t *public_key,
				  uint8_t *ciphertext, uint8_t *secret)
{
	struct workspace {
		struct lc_kyber_pk pk;
		struct lc_kyber_ct ct;
		struct lc_kyber_ss ss;
	} *ws = TEE_Malloc(sizeof(*ws), TEE_MALLOC_FILL_ZERO);
	uint8_t *bytes;
	size_t length;
	int error;

	if (!ws)
		return TEE_ERROR_OUT_OF_MEMORY;
	error = lc_kyber_pk_load(&ws->pk, public_key, pqc_pk_size(algorithm));
	if (error)
		goto out;
	error = lc_kyber_enc(&ws->ct, &ws->ss, &ws->pk);
	if (error)
		goto out;
	error = lc_kyber_ct_ptr(&bytes, &length, &ws->ct);
	if (error)
		goto out;
	if (length != pqc_ct_size(algorithm)) {
		error = -EFAULT;
		goto out;
	}
	TEE_MemMove(ciphertext, bytes, length);
	error = export_secret(&ws->ss, secret);
out:
	pqc_wipe_free(ws, sizeof(*ws));
	return convert(error);
}

TEE_Result pqc_crypto_decapsulate(uint32_t algorithm,
				  const uint8_t *private_key,
				  const uint8_t *ciphertext, uint8_t *secret)
{
	struct workspace {
		struct lc_kyber_sk sk;
		struct lc_kyber_ct ct;
		struct lc_kyber_ss ss;
	} *ws = TEE_Malloc(sizeof(*ws), TEE_MALLOC_FILL_ZERO);
	int error;

	if (!ws)
		return TEE_ERROR_OUT_OF_MEMORY;
	error = lc_kyber_sk_load(&ws->sk, private_key, pqc_sk_size(algorithm));
	if (error)
		goto out;
	error = lc_kyber_ct_load(&ws->ct, ciphertext, pqc_ct_size(algorithm));
	if (error)
		goto out;
	error = lc_kyber_dec(&ws->ss, &ws->ct, &ws->sk);
	if (!error)
		error = export_secret(&ws->ss, secret);
out:
	pqc_wipe_free(ws, sizeof(*ws));
	return convert(error);
}

static int dsa_context(struct lc_dilithium_ctx **ctx, const uint8_t *context,
		       size_t context_len)
{
	int error = lc_dilithium_ctx_alloc(ctx);

	/* The default context selects pure ML-DSA, with randomized signing. */
	if (!error)
		lc_dilithium_ctx_userctx(*ctx, context, context_len);
	return error;
}

TEE_Result pqc_crypto_sign(uint32_t algorithm, const uint8_t *private_key,
			   const uint8_t *context, size_t context_len,
			   const uint8_t *message, size_t message_len,
			   uint8_t *signature)
{
	struct workspace {
		struct lc_dilithium_sk sk;
		struct lc_dilithium_sig sig;
	} *ws = TEE_Malloc(sizeof(*ws), TEE_MALLOC_FILL_ZERO);
	struct lc_dilithium_ctx *ctx = NULL;
	uint8_t *bytes;
	size_t length;
	int error;

	if (!ws)
		return TEE_ERROR_OUT_OF_MEMORY;
	error = dsa_context(&ctx, context, context_len);
	if (error)
		goto out;
	error = lc_dilithium_sk_load(&ws->sk, private_key,
				     pqc_sk_size(algorithm));
	if (error)
		goto out;
	error = lc_dilithium_sign_ctx(&ws->sig, ctx, message, message_len,
				      &ws->sk, lc_seeded_rng);
	if (error)
		goto out;
	error = lc_dilithium_sig_ptr(&bytes, &length, &ws->sig);
	if (error)
		goto out;
	if (length != pqc_sig_size(algorithm)) {
		error = -EFAULT;
		goto out;
	}
	TEE_MemMove(signature, bytes, length);
out:
	lc_dilithium_ctx_zero_free(ctx);
	pqc_wipe_free(ws, sizeof(*ws));
	return convert(error);
}

TEE_Result pqc_crypto_verify(uint32_t algorithm, const uint8_t *public_key,
			     const uint8_t *signature, const uint8_t *context,
			     size_t context_len, const uint8_t *message,
			     size_t message_len)
{
	struct workspace {
		struct lc_dilithium_pk pk;
		struct lc_dilithium_sig sig;
	} *ws = TEE_Malloc(sizeof(*ws), TEE_MALLOC_FILL_ZERO);
	struct lc_dilithium_ctx *ctx = NULL;
	int error;

	if (!ws)
		return TEE_ERROR_OUT_OF_MEMORY;
	error = dsa_context(&ctx, context, context_len);
	if (error)
		goto out;
	error = lc_dilithium_pk_load(&ws->pk, public_key,
				     pqc_pk_size(algorithm));
	if (error)
		goto out;
	error = lc_dilithium_sig_load(&ws->sig, signature,
				      pqc_sig_size(algorithm));
	if (error)
		goto out;
	error = lc_dilithium_verify_ctx(&ws->sig, ctx, message, message_len,
					&ws->pk);
out:
	lc_dilithium_ctx_zero_free(ctx);
	pqc_wipe_free(ws, sizeof(*ws));
	return convert(error);
}
