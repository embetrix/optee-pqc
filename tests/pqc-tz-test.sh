#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (C) 2026 Embetrix Embedded Systems Solutions <ayoub.zaki@embetrix.com>

set -eu
umask 077
export PQC_TZ_TIMING="${PQC_TZ_TIMING:-1}"
export LC_ALL=C

if [ "$#" -ne 0 ]; then
    echo "Usage: pqc-tz-test"
    echo "Test all six algorithms and delete the test keys and working files"
    echo "Benchmark signing/verifying from 0 to 65536 bytes and fixed-size KEM inputs"
    echo "PQC_BENCH_RUNS=5: measured runs per case (1-1000), plus one warm-up"
    echo "PQC_BENCH_DIR: new directory for CSV reports (default: temporary directory)"
    echo "PQC_TZ_TIMING=0: correctness tests only, without benchmarks"
    echo "Run a separate reboot test to check key persistence"
    [ "$#" -eq 1 ] && [ "$1" = "--help" ] && exit 0
    exit 1
fi

command -v pqc_tz_client >/dev/null 2>&1 || {
    echo "FAIL: pqc_tz_client is missing" >&2
    exit 1
}

work=$(mktemp -d "${TMPDIR:-/tmp}/pqc-tz-test.XXXXXX")
key_id=
results_dir=

cleanup() {
    status=$?
    trap - 0 HUP INT TERM
    if [ -n "$key_id" ]; then
        if ! pqc_tz_client delete "$key_id"; then
            echo "FAIL: could not delete test key $key_id" >&2
            status=1
        fi
    fi
    rm -rf "$work" || status=1
    if [ "$status" -eq 0 ]; then
        echo "PASS: all PQC tests passed"
    else
        echo "FAIL: PQC test failed" >&2
        if [ -n "$results_dir" ]; then
            echo "Incomplete benchmark results: $results_dir" >&2
        fi
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

# Capture only the requested invocation, never CLI startup or file I/O.
benchmark_command() {
    bench_operation=$1
    bench_kind=$2
    bench_bytes=$3
    shift 3
    if ! pqc_tz_client "$bench_operation" "$@" \
        > "$work/bench-stdout" 2> "$work/bench-stderr"; then
        cat "$work/bench-stdout" "$work/bench-stderr" >&2
        fail "benchmark $algorithm $bench_operation failed"
    fi
    if ! bench_ms=$(awk -v operation="$bench_operation" '
        $1 == "pqc_tz_client:" && $2 == operation &&
        $3 == "invoke" && $4 == "time:" &&
        $5 ~ /^[0-9]+\.[0-9]+$/ && $6 == "ms" &&
        $7 == "(result=0x00000000)" { value = $5; count++ }
        END { if (count != 1) exit 1; print value }
    ' "$work/bench-stderr"); then
        cat "$work/bench-stderr" >&2
        fail "missing or ambiguous timing for $algorithm $bench_operation"
    fi
    if [ "$bench_iteration" -gt 0 ]; then
        printf '%s,%s,%s,%s,%s,%s\n' "$algorithm" "$bench_operation" \
            "$bench_kind" "$bench_bytes" "$bench_iteration" "$bench_ms" \
            >> "$results_dir/samples.csv"
    fi
}

benchmark_signatures() {
    # Keep the upper bound in sync with PQC_MAX_MESSAGE in pqc_protocol.h.
    for message_bytes in 0 1 32 256 1024 4096 16384 32768 65536; do
        echo "Benchmarking $algorithm: $message_bytes message bytes"
        if [ "$message_bytes" -eq 0 ]; then
            : > "$test_dir/bench-message"
        else
            dd if=/dev/zero of="$test_dir/bench-message" \
                bs="$message_bytes" count=1 2>/dev/null
        fi
        bench_iteration=0
        while [ "$bench_iteration" -le "$bench_runs" ]; do
            rm -f "$test_dir/bench-signature"
            benchmark_command sign message "$message_bytes" "$key_id" \
                "$test_dir/bench-message" "$test_dir/bench-signature"
            benchmark_command verify message "$message_bytes" "$test_dir/public" \
                "$test_dir/bench-message" "$test_dir/bench-signature"
            bench_iteration=$((bench_iteration + 1))
        done
    done
}

benchmark_kem() {
    public_bytes=$(wc -c < "$test_dir/public" | tr -d '[:space:]')
    echo "Benchmarking $algorithm: fixed-size public key and ciphertext"
    bench_iteration=0
    while [ "$bench_iteration" -le "$bench_runs" ]; do
        rm -f "$test_dir/bench-ciphertext" "$test_dir/bench-secret" \
            "$test_dir/bench-recovered"
        benchmark_command encap public_key "$public_bytes" "$test_dir/public" \
            "$test_dir/bench-ciphertext" "$test_dir/bench-secret"
        ciphertext_bytes=$(wc -c < "$test_dir/bench-ciphertext" | tr -d '[:space:]')
        benchmark_command decap ciphertext "$ciphertext_bytes" "$key_id" \
            "$test_dir/bench-ciphertext" "$test_dir/bench-recovered"
        check_secret "$test_dir/bench-secret"
        check_secret "$test_dir/bench-recovered"
        cmp -s "$test_dir/bench-secret" "$test_dir/bench-recovered" ||
            fail "benchmark shared secrets differ"
        bench_iteration=$((bench_iteration + 1))
    done
}

benchmark_report() {
    # Sort samples within each case so the median works with odd/even run counts.
    sed '1d' "$results_dir/samples.csv" > "$work/samples"
    sort -t , -k1,1 -k2,2 -k3,3 -k4,4n -k6,6n "$work/samples" > "$work/sorted"
    awk -F, '
        function emit(    median) {
            if (!n) return
            median = n % 2 ? values[(n + 1) / 2] :
                (values[n / 2] + values[n / 2 + 1]) / 2
            printf "%s,%d,%.3f,%.3f,%.3f,%.3f\n", group, n,
                values[1], median, total / n, values[n]
        }
        BEGIN { print "algorithm,operation,input_kind,input_bytes,runs,min_ms,median_ms,mean_ms,max_ms" }
        {
            next_group = $1 "," $2 "," $3 "," $4
            if (next_group != group) {
                emit(); n = 0; total = 0; group = next_group
            }
            values[++n] = $6 + 0; total += $6
        }
        END { emit() }
    ' "$work/sorted" > "$results_dir/summary.csv"
    printf '\nInvocation time in milliseconds (lower is faster)\n'
    echo "One warm-up excluded per case; $bench_runs measured runs; zero-filled messages, no context."
    echo "Includes invocation overhead and TA storage access; excludes session setup and client file I/O."
    awk -F, '
        NR == 1 {
            printf "%-10s %-7s %-10s %7s %5s %10s %10s %10s %10s\n",
                "Algorithm", "Op", "Input", "Bytes", "Runs", "Min", "Median", "Average", "Max"
            next
        }
        { printf "%-10s %-7s %-10s %7s %5s %10s %10s %10s %10s\n",
            $1, $2, $3, $4, $5, $6, $7, $8, $9 }
    ' "$results_dir/summary.csv"
    echo "CSV reports: $results_dir/summary.csv and $results_dir/samples.csv"
}

if [ "$PQC_TZ_TIMING" = 1 ]; then
    bench_runs=${PQC_BENCH_RUNS:-5}
    case "$bench_runs" in
        [1-9]|[1-9][0-9]|[1-9][0-9][0-9]|1000) ;;
        *) fail "PQC_BENCH_RUNS must be an integer from 1 to 1000" ;;
    esac
    if [ -n "${PQC_BENCH_DIR:-}" ]; then
        mkdir -- "$PQC_BENCH_DIR" || fail "PQC_BENCH_DIR must be a new directory"
        results_dir=$PQC_BENCH_DIR
    else
        results_dir=$(mktemp -d "${TMPDIR:-/tmp}/pqc-tz-bench.XXXXXX")
    fi
    echo "Benchmark reports: $results_dir"
    printf 'algorithm,operation,input_kind,input_bytes,iteration,invoke_ms\n' \
        > "$results_dir/samples.csv"
else
    echo "Benchmarks disabled (PQC_TZ_TIMING is not 1)"
fi

for algorithm in mldsa44 mldsa65 mldsa87 mlkem512 mlkem768 mlkem1024; do
    echo "Testing $algorithm"
    test_dir="$work/$algorithm"
    mkdir "$test_dir"
    key_id=$(pqc_tz_client generate "$algorithm")
    case "$key_id" in
        ''|*[!0-9a-fA-F]*) fail "invalid key ID returned: $key_id" ;;
    esac
    [ "${#key_id}" -eq 32 ] || fail "invalid key ID length"
    pqc_tz_client list > "$test_dir/keys"
    grep -q "^$key_id " "$test_dir/keys" || fail "new key is missing from list"
    pqc_tz_client public "$key_id" "$test_dir/public"

    case "$algorithm" in
        mldsa*)
            printf 'OP-TEE PQC test\n' > "$test_dir/message"
            printf 'Changed message\n' > "$test_dir/changed-message"
            printf 'test' > "$test_dir/context"
            printf 'other-context' > "$test_dir/changed-context"

            pqc_tz_client sign "$key_id" "$test_dir/message" "$test_dir/signature"
            pqc_tz_client verify "$test_dir/public" "$test_dir/message" "$test_dir/signature"
            expect_error 0xffff3072 pqc_tz_client verify \
                "$test_dir/public" "$test_dir/changed-message" "$test_dir/signature"

            pqc_tz_client sign "$key_id" "$test_dir/message" \
                "$test_dir/context-signature" "$test_dir/context"
            pqc_tz_client verify "$test_dir/public" "$test_dir/message" \
                "$test_dir/context-signature" "$test_dir/context"
            expect_error 0xffff3072 pqc_tz_client verify \
                "$test_dir/public" "$test_dir/message" \
                "$test_dir/context-signature" "$test_dir/changed-context"
            if [ "$PQC_TZ_TIMING" = 1 ]; then
                benchmark_signatures
            fi
            ;;
        mlkem*)
            pqc_tz_client encap "$test_dir/public" "$test_dir/ciphertext" "$test_dir/secret"
            pqc_tz_client decap "$key_id" "$test_dir/ciphertext" "$test_dir/recovered"
            check_secret "$test_dir/secret"
            check_secret "$test_dir/recovered"
            cmp -s "$test_dir/secret" "$test_dir/recovered" || fail "shared secrets differ"

            # ML-KEM returns a different secret for a damaged ciphertext
            cp "$test_dir/ciphertext" "$test_dir/changed-ciphertext"
            byte=$(od -An -tu1 -N1 "$test_dir/ciphertext")
            octal=$(printf '%03o' "$((byte ^ 1))")
            printf '%b' "\\0$octal" | dd of="$test_dir/changed-ciphertext" \
                bs=1 count=1 conv=notrunc 2>/dev/null
            pqc_tz_client decap "$key_id" "$test_dir/changed-ciphertext" "$test_dir/rejected-secret"
            check_secret "$test_dir/rejected-secret"
            if cmp -s "$test_dir/secret" "$test_dir/rejected-secret"; then
                fail "damaged ciphertext produced the original secret"
            fi
            if [ "$PQC_TZ_TIMING" = 1 ]; then
                benchmark_kem
            fi
            ;;
    esac

    pqc_tz_client delete "$key_id"
    deleted_id=$key_id
    key_id=
    expect_error 0xffff0008 pqc_tz_client public "$deleted_id" "$test_dir/deleted-public"
    pqc_tz_client list > "$test_dir/keys-after"
    if grep -q "^$deleted_id " "$test_dir/keys-after"; then
        fail "deleted key is still listed"
    fi
    echo "PASS: $algorithm"
done

if [ "$PQC_TZ_TIMING" = 1 ]; then
    benchmark_report
fi
