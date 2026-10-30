# optee-pqc

OP-TEE TA and Linux client for persistent post-quantum keys.

- ML-DSA: 44, 65 and 87 ([FIPS 204](https://csrc.nist.gov/pubs/fips/204/final))
- ML-KEM: 512, 768 and 1024 ([FIPS 203](https://csrc.nist.gov/pubs/fips/203/final))

Private keys stay in the TA. Shared secrets return to Linux.
[Leancrypto](https://github.com/smuellerDD/leancrypto) is built from the pinned
submodule and linked into the TA.

## Build

You need a Linux SDK with libteec and a matching OP-TEE TA devkit.
Build tools: CMake 3.22+, Make, GCC, Meson, Ninja, Python 3 and the devkit's
Python signing packages.

Set the paths for your SDK and TA devkit:

```sh
git submodule update --init external/leancrypto

cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/sdk/toolchain.cmake \
  -DPQC_BUILD_CLIENT=ON \
  -DPQC_BUILD_TA=ON \
  -DPQC_TA_DEV_KIT=/path/to/export-ta_arm64 \
  -DPQC_TA_CROSS_COMPILE=aarch64-linux-gnu-

cmake --build build
```

Optional ARM64 assembly/NEON: add `-DPQC_LEANCRYPTO_ASM=ON` to CMake
(default: `OFF`). Requires an ARM64 devkit with `CFG_TA_FLOAT_SUPPORT=y`.
For direct TA builds, use `make ... PQC_LEANCRYPTO_ASM=1`.
The pinned Leancrypto version keeps ML-KEM-512 and ML-DSA-44 PQC code in C.

## Install

```sh
DESTDIR=/path/to/staging cmake --install build --prefix /usr
```

Copy the client to the target's PATH and the signed `.ta` to its TA search
directory. The target needs OP-TEE and tee-supplicant.

## Commands

Run on the target. Output files must not already exist.

### Keys

```sh
pqc_tz_client generate mlkem768
pqc_tz_client list
pqc_tz_client public ID PUBLIC_KEY_FILE
pqc_tz_client delete ID
```

`generate` prints the key ID. Choose `mlkem512`, `mlkem768`, `mlkem1024`,
`mldsa44`, `mldsa65` or `mldsa87`.

### Shared secrets

```sh
pqc_tz_client encap PUBLIC_KEY_FILE CIPHERTEXT_FILE SECRET_FILE
pqc_tz_client decap ID CIPHERTEXT_FILE SECRET_FILE
```

### Signatures

```text
pqc_tz_client sign ID MESSAGE_FILE SIGNATURE_FILE [CONTEXT_FILE]
pqc_tz_client verify PUBLIC_KEY_FILE MESSAGE_FILE SIGNATURE_FILE [CONTEXT_FILE]
```

C API: [client/pqc_tz_client.h](client/pqc_tz_client.h).

### Timing

Set `PQC_TZ_TIMING=1` to print invocation time in milliseconds to stderr:

```sh
PQC_TZ_TIMING=1 pqc_tz_client generate mlkem768
```

Includes invocation overhead and TA processing; excludes session setup and client
file I/O. Disabled by default for standalone client commands.

## Tests and benchmarks

Run on the OP-TEE target with `pqc_tz_client` in `PATH`:

```sh
sh tests/pqc-tz-test.sh
# Customize measured runs (default: 5) and save reports in a new directory:
PQC_BENCH_RUNS=20 PQC_BENCH_DIR=./benchmark-results sh tests/pqc-tz-test.sh
# Correctness tests only:
PQC_TZ_TIMING=0 sh tests/pqc-tz-test.sh
```

Tests all six algorithms. Benchmarks ML-DSA signing/verifying at nine message sizes
from 0 to 65536 bytes, and ML-KEM encapsulation/decapsulation at fixed input sizes.
One warm-up per case is excluded.

Prints min/median/average/max times in milliseconds and saves `summary.csv` and
`samples.csv` in the reported directory (temporary by default). `PQC_BENCH_DIR`
must not already exist. Test keys and working files are removed afterward.

## Security considerations

- All callers can use or delete any stored key.
- REE storage has no rollback protection.
- Keep storage files across reboots so keys are not lost.
- Linux must protect returned shared secrets.

## Disclaimer

Experimental. Do not use in production.
