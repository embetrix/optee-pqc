// SPDX-License-Identifier: BSD-2-Clause
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "pqc_client.h"

static int read_file(const char *path, uint8_t *buffer, size_t capacity,
		     size_t *length)
{
	FILE *file = fopen(path, "rb");
	int extra, failed;

	if (!file) {
		perror(path);
		return -1;
	}
	*length = fread(buffer, 1, capacity, file);
	extra = fgetc(file);
	failed = ferror(file) || extra != EOF;
	if (fclose(file))
		failed = 1;
	if (failed)
		fprintf(stderr, "Cannot read %s or file exceeds %zu bytes\n",
			path, capacity);
	return failed ? -1 : 0;
}

static int read_exact(const char *path, uint8_t *buffer, size_t length)
{
	size_t actual;

	if (read_file(path, buffer, length, &actual))
		return -1;
	if (actual == length)
		return 0;
	fprintf(stderr, "Wrong length for %s (expected %zu bytes)\n", path,
		length);
	return -1;
}

static int write_file(const char *path, const uint8_t *buffer, size_t length)
{
	FILE *file = fopen(path, "wbx");
	int failed;

	if (!file) {
		perror(path);
		return -1;
	}
	failed = fwrite(buffer, 1, length, file) != length;
	if (fclose(file))
		failed = 1;
	if (failed) {
		fprintf(stderr, "Write failed: %s\n", path);
		remove(path);
	}
	return failed ? -1 : 0;
}

static int parse_id(const char *text, uint8_t *id)
{
	if (strlen(text) != 2 * PQC_ID_SIZE)
		return -1;
	for (size_t i = 0; i < PQC_ID_SIZE; i++) {
		unsigned int byte;

		if (!strchr("0123456789abcdefABCDEF", text[2 * i]) ||
		    !strchr("0123456789abcdefABCDEF", text[2 * i + 1]) ||
		    sscanf(text + 2 * i, "%2x", &byte) != 1)
			return -1;
		id[i] = (uint8_t)byte;
	}
	return 0;
}

static void print_id(const uint8_t *id)
{
	for (size_t i = 0; i < PQC_ID_SIZE; i++)
		printf("%02x", id[i]);
}

static int usage(void)
{
	fputs("Usage:\n"
	      "  pqc-client generate mlkem512|mlkem768|mlkem1024|mldsa44|mldsa65|mldsa87\n"
	      "  pqc-client list\n"
	      "  pqc-client public ID PUBLIC_KEY_FILE\n"
	      "  pqc-client delete ID\n"
	      "  pqc-client encap PUBLIC_KEY_FILE CIPHERTEXT_FILE SECRET_FILE\n"
	      "  pqc-client decap ID CIPHERTEXT_FILE SECRET_FILE\n"
	      "  pqc-client sign ID MESSAGE_FILE SIGNATURE_FILE [CONTEXT_FILE]\n"
	      "  pqc-client verify PUBLIC_KEY_FILE MESSAGE_FILE SIGNATURE_FILE [CONTEXT_FILE]\n"
	      "Aliases: mlkem=mlkem768, mldsa=mldsa87. Output files must not exist. Messages are limited to 65536 bytes.\n",
	      stderr);
	return EXIT_FAILURE;
}

struct cli {
	struct pqc_client client;
	uint8_t id[PQC_ID_SIZE], pk[PQC_MAX_PK_SIZE], ct[PQC_MAX_CT_SIZE];
	uint8_t secret[PQC_SECRET_SIZE], signature[PQC_MAX_SIG_SIZE];
	uint8_t message[PQC_MAX_MESSAGE], context[PQC_MAX_CONTEXT];
	uint8_t page[PQC_LIST_PAGE_SIZE];
};

static int report_error(struct cli *cli, TEEC_Result result, bool opening)
{
	const char *reason = pqc_result_string(result);
	uint32_t origin = cli->client.error_origin;

	if (!result)
		return EXIT_SUCCESS;
	if (result == TEEC_ERROR_ITEM_NOT_FOUND) {
		if (opening)
			reason =
				"Trusted application unavailable; check that the TA is installed";
		else if (origin == TEEC_ORIGIN_TRUSTED_APP)
			reason = "Key not found";
	}
	/* Retain the numeric line for scripts; include origin for diagnosis. */
	fprintf(stderr, "OP-TEE operation failed: 0x%08x\n", result);
	fprintf(stderr, "Reason: %s; origin: %s (0x%08x)\n", reason,
		pqc_origin_string(origin), origin);
	return EXIT_FAILURE;
}

static int connect(struct cli *cli)
{
	return report_error(cli, pqc_open(&cli->client), true);
}

static int key_id(const char *text, uint8_t *id)
{
	if (!parse_id(text, id))
		return 0;
	fputs("Invalid key ID\n", stderr);
	return -1;
}

static int read_public(const char *path, uint8_t *pk, bool kem,
		       uint32_t *algorithm)
{
	size_t length;
	if (read_file(path, pk, PQC_MAX_PK_SIZE, &length))
		return -1;
	for (uint32_t id = 1; id <= PQC_ALGORITHM_COUNT; id++) {
		if (pqc_pk_size(id) == length &&
		    (kem ? pqc_ct_size(id) : pqc_sig_size(id))) {
			*algorithm = id;
			return 0;
		}
	}
	fprintf(stderr, "Unsupported %s public-key length in %s\n",
		kem ? "ML-KEM" : "ML-DSA", path);
	return -1;
}

static int command_generate(struct cli *cli, int argc, char **args)
{
	uint32_t algorithm = 0;
	(void)argc;

	if ((!strcmp(args[0], "mlkem") || !strcmp(args[0], "kem")))
		algorithm = PQC_MLKEM768;
	else if ((!strcmp(args[0], "mldsa") || !strcmp(args[0], "dsa")))
		algorithm = PQC_MLDSA87;
	else {
		for (uint32_t id = 1; id <= PQC_ALGORITHM_COUNT; id++)
			if (!strcmp(args[0], pqc_algorithm(id)->cli_name))
				algorithm = id;
	}
	if (!algorithm)
		return usage();
	if (connect(cli) ||
	    report_error(cli, pqc_generate(&cli->client, algorithm, cli->id),
			 false))
		return EXIT_FAILURE;
	print_id(cli->id);
	putchar('\n');
	return EXIT_SUCCESS;
}

static int command_list(struct cli *cli, int argc, char **args)
{
	(void)argc;
	(void)args;
	if (connect(cli))
		return EXIT_FAILURE;
	for (bool reset = true;; reset = false) {
		size_t length = sizeof(cli->page);
		TEEC_Result result =
			pqc_list(&cli->client, reset, cli->page, &length);

		if (report_error(cli, result, false))
			return EXIT_FAILURE;
		if (length % PQC_LIST_ENTRY_SIZE) {
			fputs("Invalid key-list response from TA\n", stderr);
			return EXIT_FAILURE;
		}
		for (size_t i = 0; i < length; i += PQC_LIST_ENTRY_SIZE) {
			uint32_t algorithm =
				pqc_get32(cli->page + i + PQC_ID_SIZE);

			if (!pqc_algorithm(algorithm)) {
				fputs("Unknown key algorithm in TA response\n",
				      stderr);
				return EXIT_FAILURE;
			}
			print_id(cli->page + i);
			printf(" %s\n", pqc_algorithm(algorithm)->name);
		}
		if (length < sizeof(cli->page))
			return EXIT_SUCCESS;
	}
}

static int command_public(struct cli *cli, int argc, char **args)
{
	uint32_t algorithm;
	size_t length = sizeof(cli->pk);
	(void)argc;

	if (key_id(args[0], cli->id) || connect(cli) ||
	    report_error(cli,
			 pqc_public(&cli->client, cli->id, &algorithm, cli->pk,
				    &length),
			 false))
		return EXIT_FAILURE;
	return write_file(args[1], cli->pk, length) ? EXIT_FAILURE
						    : EXIT_SUCCESS;
}

static int command_delete(struct cli *cli, int argc, char **args)
{
	(void)argc;
	if (key_id(args[0], cli->id) || connect(cli))
		return EXIT_FAILURE;
	return report_error(cli, pqc_delete(&cli->client, cli->id), false);
}

static int command_encapsulate(struct cli *cli, int argc, char **args)
{
	uint32_t algorithm;
	size_t length = sizeof(cli->ct);
	(void)argc;
	if (read_public(args[0], cli->pk, true, &algorithm) || connect(cli) ||
	    report_error(cli,
			 pqc_encapsulate_ex(&cli->client, algorithm, cli->pk,
					    pqc_pk_size(algorithm), cli->ct,
					    &length, cli->secret),
			 false))
		return EXIT_FAILURE;
	if (write_file(args[1], cli->ct, length) ||
	    write_file(args[2], cli->secret, sizeof(cli->secret)))
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

static int command_decapsulate(struct cli *cli, int argc, char **args)
{
	size_t length;
	(void)argc;
	if (key_id(args[0], cli->id) ||
	    read_file(args[1], cli->ct, sizeof(cli->ct), &length) ||
	    connect(cli) ||
	    report_error(cli,
			 pqc_decapsulate_ex(&cli->client, cli->id, cli->ct,
					    length, cli->secret),
			 false))
		return EXIT_FAILURE;
	return write_file(args[2], cli->secret, sizeof(cli->secret))
		       ? EXIT_FAILURE
		       : EXIT_SUCCESS;
}

static int read_message(struct cli *cli, int argc, char **args,
			size_t *message_len, size_t *context_len)
{
	*context_len = 0;
	if (read_file(args[1], cli->message, sizeof(cli->message), message_len))
		return -1;
	if (argc == 4 &&
	    read_file(args[3], cli->context, sizeof(cli->context), context_len))
		return -1;
	return 0;
}

static int command_sign(struct cli *cli, int argc, char **args)
{
	size_t message_len, context_len, signature_len = sizeof(cli->signature);

	if (key_id(args[0], cli->id) ||
	    read_message(cli, argc, args, &message_len, &context_len) ||
	    connect(cli))
		return EXIT_FAILURE;
	if (report_error(cli,
			 pqc_sign_ex(&cli->client, cli->id, cli->message,
				     message_len, cli->context, context_len,
				     cli->signature, &signature_len),
			 false))
		return EXIT_FAILURE;
	return write_file(args[2], cli->signature, signature_len)
		       ? EXIT_FAILURE
		       : EXIT_SUCCESS;
}

static int command_verify(struct cli *cli, int argc, char **args)
{
	size_t message_len, context_len;
	uint32_t algorithm;

	if (read_public(args[0], cli->pk, false, &algorithm) ||
	    read_exact(args[2], cli->signature, pqc_sig_size(algorithm)) ||
	    read_message(cli, argc, args, &message_len, &context_len) ||
	    connect(cli))
		return EXIT_FAILURE;
	if (report_error(cli,
			 pqc_verify_ex(&cli->client, algorithm, cli->pk,
				       pqc_pk_size(algorithm), cli->message,
				       message_len, cli->context, context_len,
				       cli->signature, pqc_sig_size(algorithm)),
			 false))
		return EXIT_FAILURE;
	puts("Signature verified");
	return EXIT_SUCCESS;
}

static const struct command {
	const char *name;
	int min_args, max_args;
	int (*run)(struct cli *cli, int argc, char **args);
} commands[] = {
	{ "generate", 1, 1, command_generate },
	{ "list", 0, 0, command_list },
	{ "public", 2, 2, command_public },
	{ "delete", 1, 1, command_delete },
	{ "encap", 3, 3, command_encapsulate },
	{ "decap", 3, 3, command_decapsulate },
	{ "sign", 3, 4, command_sign },
	{ "verify", 3, 4, command_verify },
};

int main(int argc, char **argv)
{
	const struct command *command = NULL;
	struct cli *cli;
	int status;

	if (argc < 2)
		return usage();
	for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
		if (!strcmp(argv[1], commands[i].name)) {
			command = &commands[i];
			break;
		}
	}
	if (!command || argc - 2 < command->min_args ||
	    argc - 2 > command->max_args)
		return usage();
	cli = calloc(1, sizeof(*cli));
	if (!cli) {
		fputs("Out of memory\n", stderr);
		return EXIT_FAILURE;
	}
	umask(077);
	status = command->run(cli, argc - 2, argv + 2);
	pqc_close(&cli->client);
	pqc_clear(cli, sizeof(*cli));
	free(cli);
	return status;
}
