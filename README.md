# optee-pqc

Post-quantum cryptography in an OP-TEE Trusted Application (TA) using
[leancrypto](https://github.com/smuellerDD/leancrypto), with a Linux command-line
client and C API.

The TA links leancrypto directly and exposes operations through a custom command
interface. Private keys stay in the TA. KEM shared secrets are returned to Linux.

| Algorithm | Parameter sets | Operations |
| --- | --- | --- |
| ML-KEM | 512, 768, 1024 | Key generation, encapsulation, decapsulation |
| ML-DSA | 44, 65, 87 | Key generation, signing, verification |

> Experimental. Do not use in production. See [Security limitations](#security-limitations).

## Build

### Prerequisites

- Linux SDK with libteec and a matching OP-TEE TA devkit.
- CMake 3.22+, Make, GCC, Meson, Ninja and Python 3.
- Python signing dependencies required by the TA devkit.

### Compile and install

Initialize the leancrypto submodule, then configure the build with your SDK and
TA devkit paths. This example targets ARM64.

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

Deploy the staged client to a directory in the target's `PATH` and the `.ta` file
into its TA search directory. The target must run OP-TEE and `tee-supplicant`.

### ARM64 assembly

Assembly is disabled by default. To enable it, add `-DPQC_LEANCRYPTO_ASM=ON` to the
CMake command and use a TA devkit built with `CFG_TA_FLOAT_SUPPORT=y`.
ML-KEM-512 and ML-DSA-44 still use C for their PQC code.

## Usage

The client accepts the following algorithm names.

- ML-KEM `mlkem512`, `mlkem768`, `mlkem1024`
- ML-DSA `mldsa44`, `mldsa65`, `mldsa87`

In the examples below, `ID` is the key ID printed by `generate`. The other
uppercase arguments are file paths. Output files must not already exist.

### Manage keys

```sh
pqc_tz_client generate mlkem768
pqc_tz_client list
pqc_tz_client public ID PUBLIC_KEY
pqc_tz_client delete ID
```

### Encapsulate and decapsulate

Use an ML-KEM key to encapsulate a shared secret and recover it by decapsulation.

```sh
pqc_tz_client encap PUBLIC_KEY CIPHERTEXT SECRET
pqc_tz_client decap ID CIPHERTEXT SECRET
```

### Sign and verify

Use an ML-DSA key for signatures. The context file is optional.

```sh
pqc_tz_client sign ID MESSAGE SIGNATURE [CONTEXT]
pqc_tz_client verify PUBLIC_KEY MESSAGE SIGNATURE [CONTEXT]
```

For integration into a Linux application, use the C API in
[pqc_tz_client.h](client/pqc_tz_client.h).

## Tests and benchmarks

Copy [pqc-tz-test.sh](tests/pqc-tz-test.sh) to a directory in the target's `PATH`.
Run it with the options below.

```sh
# Run functional tests and benchmarks
pqc-tz-test.sh

# Use 50 measured runs per case and save reports to ./results
PQC_BENCH_RUNS=50 PQC_BENCH_DIR=./results \
  pqc-tz-test.sh

# Run functional tests only
PQC_TZ_TIMING=0 pqc-tz-test.sh
```

The suite covers all six parameter sets, with ML-DSA messages from 0 to 64 KiB
and fixed-size ML-KEM inputs. By default, each benchmark case runs once to warm
up, followed by five measured runs. The TA stays alive between calls.

Results include minimum, median, average and maximum times in milliseconds.
The script saves `summary.csv` and `samples.csv` to the printed report directory.
`PQC_BENCH_DIR` must name a directory that does not already exist. If omitted,
reports go to a temporary directory. Test keys and working files are removed.

Timing includes command invocation and TA execution, excluding session setup and
client file I/O. Set `PQC_TZ_TIMING=1` to print timing for individual client commands.

## Security limitations

- All callers can use or delete any stored key.
- REE storage has no rollback protection.
- Storage files must be preserved across reboots to retain keys.
- Linux is responsible for protecting returned shared secrets.
