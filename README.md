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
pqc-client generate mlkem768
pqc-client list
pqc-client public ID PUBLIC_KEY_FILE
pqc-client delete ID
```

`generate` prints the key ID. Choose `mlkem512`, `mlkem768`, `mlkem1024`,
`mldsa44`, `mldsa65` or `mldsa87`.

### Shared secrets

```sh
pqc-client encap PUBLIC_KEY_FILE CIPHERTEXT_FILE SECRET_FILE
pqc-client decap ID CIPHERTEXT_FILE SECRET_FILE
```

### Signatures

```text
pqc-client sign ID MESSAGE_FILE SIGNATURE_FILE [CONTEXT_FILE]
pqc-client verify PUBLIC_KEY_FILE MESSAGE_FILE SIGNATURE_FILE [CONTEXT_FILE]
```

C API: [client/pqc_client.h](client/pqc_client.h).

## Security considerations

- All callers can use or delete any stored key.
- REE storage has no rollback protection.
- Keep storage files across reboots so keys are not lost.
- Linux must protect returned shared secrets.

## Disclaimer

Experimental. Do not use in production.
