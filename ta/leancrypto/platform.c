// SPDX-License-Identifier: BSD-2-Clause
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
/*
 * OP-TEE backing for the platform interfaces leancrypto expects from its host.
 *
 * lc_seeded_rng is bound straight to TEE_GenerateRandom rather than seeding
 * leancrypto's own DRNG from it: OP-TEE core already owns a CSPRNG, so a TA
 * has no reason to hold expanded DRBG state of its own. This replaces
 * upstream's seeded_rng.c, which ta/leancrypto/build.py excludes from the
 * adapted archive.
 */
#include "ext_headers.h"
#include "initialization.h"
#include "lc_rng.h"
#include "seeded_rng.h"

/* All runtime RNG paths, including implicit KEM RNG use, end here. */
static int generate(void *state, const uint8_t *additional,
		    size_t additional_len, uint8_t *out, size_t size)
{
	(void)state;
	(void)additional;
	(void)additional_len;

	if (!out && size)
		return -EINVAL;

	if (size)
		TEE_GenerateRandom(out, size);

	return 0;
}

static const struct lc_rng tee_rng = { .generate = generate };
static struct lc_rng_ctx tee_rng_ctx = { .rng = &tee_rng };

struct lc_rng_ctx *lc_seeded_rng = &tee_rng_ctx;
struct lc_rng_ctx *lc_seeded_rng_internal = &tee_rng_ctx;

void lc_seeded_rng_zero_state(void)
{
	/* The RNG state is owned by OP-TEE core. No TA-local state exists. */
}

void lc_seeded_rng_status(char *buf, size_t len)
{
	if (len)
		snprintf(buf, len, "OP-TEE TEE_GenerateRandom\n");
}
