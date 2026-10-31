# optee-pqc

OP-TEE TA and Linux client using
[Leancrypto][leancrypto].

- ML-KEM: 512, 768 and 1024
- ML-DSA: 44, 65 and 87

Private keys stay in the TA. Shared secrets return
to Linux. Experimental. Do not use in production.

## Build

Needs a Linux SDK with libteec and a matching
OP-TEE TA devkit. Tools: CMake 3.22+, Make, GCC,
Meson, Ninja and Python 3. Install the devkit's
Python signing packages too.

```sh
git submodule update --init external/leancrypto

cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=/sdk/toolchain.cmake \
  -DPQC_BUILD_CLIENT=ON \
  -DPQC_BUILD_TA=ON \
  -DPQC_TA_DEV_KIT=/path/to/export-ta_arm64 \
  -DPQC_TA_CROSS_COMPILE=aarch64-linux-gnu-

cmake --build build
DESTDIR=/path/to/staging \
  cmake --install build --prefix /usr
```

Copy the client to the target's PATH. Copy the
`.ta` file to the target's TA search directory.
The target needs OP-TEE and tee-supplicant.

Assembly is off by default. Add
`-DPQC_LEANCRYPTO_ASM=ON` to enable it on ARM64.
The devkit needs `CFG_TA_FLOAT_SUPPORT=y`.
ML-KEM-512 and ML-DSA-44 still use C for their PQC
code.

## Commands

`generate` prints the key ID. Algorithm names:
`mlkem512`, `mlkem768`, `mlkem1024`, `mldsa44`,
`mldsa65`, `mldsa87`.

Output files must not already exist. Context is
optional.

```sh
pqc_tz_client generate mlkem768
pqc_tz_client list
pqc_tz_client public ID PUBLIC_KEY
pqc_tz_client delete ID

pqc_tz_client encap PUBLIC_KEY CIPHERTEXT SECRET
pqc_tz_client decap ID CIPHERTEXT SECRET

pqc_tz_client sign ID MESSAGE SIGNATURE [CONTEXT]
pqc_tz_client verify PUBLIC_KEY MESSAGE \
  SIGNATURE [CONTEXT]
```

C API: [pqc_tz_client.h](client/pqc_tz_client.h).

## Tests

Copy [pqc-tz-test.sh](tests/pqc-tz-test.sh) to the
target's PATH.

```sh
pqc-tz-test.sh

PQC_BENCH_RUNS=50 PQC_BENCH_DIR=./results \
  pqc-tz-test.sh

# Skip benchmarks
PQC_TZ_TIMING=0 pqc-tz-test.sh
```

Tests all six algorithms. ML-DSA messages range
from 0 to 64 KiB. ML-KEM uses fixed input sizes.
Each case has 5 measured runs by default, after
one warm-up. The TA stays alive between calls.

Prints min, median, average and max times in
milliseconds. Saves `summary.csv` and
`samples.csv` in the printed directory.
`PQC_BENCH_DIR` must not already exist. Without
it, reports go to a temporary directory. Test keys
and working files are removed.

Timing includes the command call and TA work. It
excludes session setup and client file I/O. Set
`PQC_TZ_TIMING=1` to print timing for individual
client commands.

## Limits

- All callers can use or delete any stored key.
- REE storage has no rollback protection.
- Keep storage files across reboots to keep keys.
- Linux must protect returned shared secrets.

[leancrypto]:
  https://github.com/smuellerDD/leancrypto
