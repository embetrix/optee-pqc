/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef PQC_ALGORITHMS_H
#define PQC_ALGORITHMS_H
#include "pqc_protocol.h"

struct pqc_algorithm {
	uint32_t id;
	const char *name;
	const char *cli_name;
	size_t public_size, private_size, ciphertext_size, signature_size;
};

static inline const struct pqc_algorithm *pqc_algorithm(uint32_t id)
{
	static const struct pqc_algorithm algorithms[] = {
		{ PQC_MLKEM768, "ML-KEM-768", "mlkem768", 1184, 2400, 1088, 0 },
		{ PQC_MLDSA87, "ML-DSA-87", "mldsa87", 2592, 4896, 0, 4627 },
		{ PQC_MLKEM512, "ML-KEM-512", "mlkem512", 800, 1632, 768, 0 },
		{ PQC_MLKEM1024, "ML-KEM-1024", "mlkem1024", 1568, 3168, 1568,
		  0 },
		{ PQC_MLDSA44, "ML-DSA-44", "mldsa44", 1312, 2560, 0, 2420 },
		{ PQC_MLDSA65, "ML-DSA-65", "mldsa65", 1952, 4032, 0, 3309 },
	};
	for (size_t i = 0; i < sizeof(algorithms) / sizeof(algorithms[0]); i++)
		if (algorithms[i].id == id)
			return &algorithms[i];
	return NULL;
}

static inline size_t pqc_pk_size(uint32_t id)
{
	const struct pqc_algorithm *alg = pqc_algorithm(id);
	return alg ? alg->public_size : 0;
}
static inline size_t pqc_sk_size(uint32_t id)
{
	const struct pqc_algorithm *alg = pqc_algorithm(id);
	return alg ? alg->private_size : 0;
}
static inline size_t pqc_ct_size(uint32_t id)
{
	const struct pqc_algorithm *alg = pqc_algorithm(id);
	return alg ? alg->ciphertext_size : 0;
}
static inline size_t pqc_sig_size(uint32_t id)
{
	const struct pqc_algorithm *alg = pqc_algorithm(id);
	return alg ? alg->signature_size : 0;
}
#endif
