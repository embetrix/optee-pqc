/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * (C) Copyright 2026
 * Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
 *
 */
#ifndef USER_TA_HEADER_DEFINES_H
#define USER_TA_HEADER_DEFINES_H
#include "pqc_protocol.h"
#define TA_UUID PQC_TA_UUID
/* Single instance serializes commands; sessions own their enumeration state. */
/* Retain Leancrypto initialization and self-test state between sessions. */
#define TA_FLAGS (TA_FLAG_SINGLE_INSTANCE | TA_FLAG_MULTI_SESSION | \
		  TA_FLAG_INSTANCE_KEEP_ALIVE)
#define TA_STACK_SIZE (32 * 1024)
#define TA_DATA_SIZE (1024 * 1024)
#define TA_VERSION "0.2.0"
#define TA_DESCRIPTION "Persistent ML-KEM and ML-DSA prototype"
#endif
