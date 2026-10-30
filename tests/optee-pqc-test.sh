#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (C) 2026 Embetrix Embedded Systems Solutions <ayoub.zaki@embetrix.com>

set -eu
umask 077

if [ "$#" -ne 0 ]; then
    echo "Usage: optee-pqc-test"
    echo "Test all six algorithms and delete the test keys and files"
    echo "Run a separate reboot test to check key persistence"
    [ "$#" -eq 1 ] && [ "$1" = "--help" ] && exit 0
    exit 1
fi

command -v pqc-client >/dev/null 2>&1 || {
    echo "FAIL: pqc-client is missing" >&2
    exit 1
}

work=$(mktemp -d "${TMPDIR:-/tmp}/optee-pqc-test.XXXXXX")
key_id=

cleanup() {
    status=$?
    trap - 0 HUP INT TERM
    if [ -n "$key_id" ]; then
        if ! pqc-client delete "$key_id"; then
            echo "FAIL: could not delete test key $key_id" >&2
            status=1
        fi
    fi
    rm -rf "$work" || status=1
    if [ "$status" -eq 0 ]; then
        echo "PASS: all PQC tests passed"
    else
        echo "FAIL: PQC test failed" >&2
    fi
    exit "$status"
}

trap cleanup 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

expect_error() {
    expected=$1
    shift
    if "$@" >"$work/error" 2>&1; then
        fail "command unexpectedly succeeded: $*"
    fi
    if ! grep -Fxq "OP-TEE operation failed: $expected" "$work/error"; then
        cat "$work/error" >&2
        fail "expected OP-TEE error $expected"
    fi
}

check_secret() {
    [ "$(wc -c < "$1")" -eq 32 ] || fail "shared secret is not 32 bytes"
}

for algorithm in mldsa44 mldsa65 mldsa87 mlkem512 mlkem768 mlkem1024; do
    echo "Testing $algorithm"
    test_dir="$work/$algorithm"
    mkdir "$test_dir"
    key_id=$(pqc-client generate "$algorithm")
    case "$key_id" in
        ''|*[!0-9a-fA-F]*) fail "invalid key ID returned: $key_id" ;;
    esac
    [ "${#key_id}" -eq 32 ] || fail "invalid key ID length"
    pqc-client list > "$test_dir/keys"
    grep -q "^$key_id " "$test_dir/keys" || fail "new key is missing from list"
    pqc-client public "$key_id" "$test_dir/public"

    case "$algorithm" in
        mldsa*)
            printf 'OP-TEE PQC test\n' > "$test_dir/message"
            printf 'Changed message\n' > "$test_dir/changed-message"
            printf 'test' > "$test_dir/context"
            printf 'other-context' > "$test_dir/changed-context"

            pqc-client sign "$key_id" "$test_dir/message" "$test_dir/signature"
            pqc-client verify "$test_dir/public" "$test_dir/message" "$test_dir/signature"
            expect_error 0xffff3072 pqc-client verify \
                "$test_dir/public" "$test_dir/changed-message" "$test_dir/signature"

            pqc-client sign "$key_id" "$test_dir/message" \
                "$test_dir/context-signature" "$test_dir/context"
            pqc-client verify "$test_dir/public" "$test_dir/message" \
                "$test_dir/context-signature" "$test_dir/context"
            expect_error 0xffff3072 pqc-client verify \
                "$test_dir/public" "$test_dir/message" \
                "$test_dir/context-signature" "$test_dir/changed-context"
            ;;
        mlkem*)
            pqc-client encap "$test_dir/public" "$test_dir/ciphertext" "$test_dir/secret"
            pqc-client decap "$key_id" "$test_dir/ciphertext" "$test_dir/recovered"
            check_secret "$test_dir/secret"
            check_secret "$test_dir/recovered"
            cmp -s "$test_dir/secret" "$test_dir/recovered" || fail "shared secrets differ"

            # ML-KEM returns a different secret for a damaged ciphertext
            cp "$test_dir/ciphertext" "$test_dir/changed-ciphertext"
            byte=$(od -An -tu1 -N1 "$test_dir/ciphertext")
            octal=$(printf '%03o' "$((byte ^ 1))")
            printf '%b' "\\0$octal" | dd of="$test_dir/changed-ciphertext" \
                bs=1 count=1 conv=notrunc 2>/dev/null
            pqc-client decap "$key_id" "$test_dir/changed-ciphertext" "$test_dir/rejected-secret"
            check_secret "$test_dir/rejected-secret"
            if cmp -s "$test_dir/secret" "$test_dir/rejected-secret"; then
                fail "damaged ciphertext produced the original secret"
            fi
            ;;
    esac

    pqc-client delete "$key_id"
    deleted_id=$key_id
    key_id=
    expect_error 0xffff0008 pqc-client public "$deleted_id" "$test_dir/deleted-public"
    pqc-client list > "$test_dir/keys-after"
    if grep -q "^$deleted_id " "$test_dir/keys-after"; then
        fail "deleted key is still listed"
    fi
    echo "PASS: $algorithm"
done
