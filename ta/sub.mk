#
# (C) Copyright 2026
# Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
#
lc-root := $(abspath $(sub-dir)/../external/leancrypto)

global-incdirs-y += ../include . leancrypto leancrypto/compat
# Meson writes the configured headers (lc_kyber_768.h, lc_dilithium_87.h, ...)
# next to their component's api directory in the build tree.
global-incdirs_ext-y += $(addprefix $(LC_BUILD)/meson/,hash/api internal/api \
                        ml-dsa/api ml-kem/api)
global-incdirs_ext-y += $(wildcard $(lc-root)/*/api)
# platform.c implements the seeded_rng.h noise-source interface.
global-incdirs_ext-y += $(lc-root)/drng/src
# Feature macros are generated from Meson's compilation database.
cppflags-y += -include $(LC_BUILD)/lc_optee_features.h
additional-compile-deps += $(LC_BUILD)/lc_optee_features.h
cppflags-y += -include $(sub-dir)/leancrypto/lc_optee_config.h
cflags-y += -fstack-usage
srcs-y += entry.c commands.c storage.c crypto.c leancrypto/platform.c leancrypto/compat.c
libdirs += $(LC_BUILD)
libnames += leancrypto-optee
libdeps += $(LC_BUILD)/libleancrypto-optee.a
