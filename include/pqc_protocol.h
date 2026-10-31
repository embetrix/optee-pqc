/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef PQC_PROTOCOL_H
#define PQC_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>

#define PQC_TA_UUID                                                    \
	{                                                              \
		0x7b1e6f40, 0x9c32, 0x4da8,                            \
		{                                                      \
			0xa5, 0x67, 0x71, 0x93, 0x0c, 0x4e, 0x82, 0xb6 \
		}                                                      \
	}
#define PQC_TA_UUID_STRING "7b1e6f40-9c32-4da8-a567-71930c4e82b6"
#define PQC_VERSION 1u
#define PQC_MLKEM768 1u
#define PQC_MLDSA87 2u
/* IDs 1 and 2 retain their original meaning for existing stored keys. */
#define PQC_MLKEM512 3u
#define PQC_MLKEM1024 4u
#define PQC_MLDSA44 5u
#define PQC_MLDSA65 6u
#define PQC_ALGORITHM_COUNT 6u
#define PQC_MAX_PK_SIZE 2592u
#define PQC_MAX_SK_SIZE 4896u
#define PQC_MAX_CT_SIZE 1568u
#define PQC_MAX_SIG_SIZE 4627u
#define PQC_ID_SIZE 16u
/* Legacy size names refer to ML-KEM-768 and ML-DSA-87. */
#define PQC_KEM_PK_SIZE 1184u
#define PQC_KEM_SK_SIZE 2400u
#define PQC_CT_SIZE 1088u
#define PQC_SECRET_SIZE 32u
#define PQC_DSA_PK_SIZE 2592u
#define PQC_DSA_SK_SIZE 4896u
#define PQC_SIG_SIZE 4627u
#define PQC_MAX_MESSAGE 65536u
#define PQC_MAX_CONTEXT 255u
#define PQC_LIST_ENTRY_SIZE 20u
#define PQC_LIST_PAGE_SIZE (16u * PQC_LIST_ENTRY_SIZE)
#define PQC_MAX_REQUEST                                                   \
	(4u + PQC_MAX_PK_SIZE + PQC_MAX_SIG_SIZE + 8u + PQC_MAX_CONTEXT + \
	 PQC_MAX_MESSAGE)
#define PQC_MAX_RESPONSE PQC_MAX_SIG_SIZE

enum pqc_command {
	PQC_GENERATE = 1,
	PQC_LIST,
	PQC_PUBLIC,
	PQC_DELETE,
	PQC_ENCAPSULATE,
	PQC_DECAPSULATE,
	PQC_SIGN,
	PQC_VERIFY,
	/* Explicit algorithm prefix; commands 1-8 keep their original framing. */
	PQC_ENCAPSULATE_ALG,
	PQC_VERIFY_ALG
};

/* Wire integers are little endian, never native structs. */
static inline uint32_t pqc_get32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
	       (uint32_t)p[3] << 24;
}

static inline void pqc_put32(uint8_t *p, uint32_t n)
{
	p[0] = (uint8_t)n;
	p[1] = (uint8_t)(n >> 8);
	p[2] = (uint8_t)(n >> 16);
	p[3] = (uint8_t)(n >> 24);
}
#endif
