#
# (C) Copyright 2026
# Embetrix Embedded Systems Solutions, ayoub.zaki@embetrix.com
#
BINARY = 7b1e6f40-9c32-4da8-a567-71930c4e82b6
CFG_TEE_TA_LOG_LEVEL ?= 1
# Never mix generated files with the TA sources, including for direct make.
O ?= $(CURDIR)/../build/ta
override O := $(shell python3 leancrypto/build.py --output-directory "$(O)")
ifeq ($(O),)
$(error O must name a separate build directory)
endif
override LC_BUILD := $(O)/leancrypto
include $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk

# Export values directly, without interpolating compiler flags into shell code.
export TA_DEV_KIT_DIR
export LC_BUILD
export LC_COMPILER_PREFIX := $(CROSS_COMPILE_$(sm))
export LC_ARCH := $(sm)
export LC_CFLAGS := $(filter-out $(CFLAGS_REMOVE) $(cflags-remove) $(cflags-remove-$(sm)),$(CFLAGS$(arch-bits-$(sm))) $(CFLAGS) $(cflags$(sm)))
export LC_CPPFLAGS := $(filter-out $(CPPFLAGS_REMOVE) $(cppflags-remove) $(cppflags-remove-$(sm)),$(CPPFLAGS) $(cppflags$(sm)))

# Build the archive and its generated headers before compiling the TA.
.PHONY: leancrypto
leancrypto:
	python3 '$(abspath $(CURDIR)/leancrypto)/build.py'

$(LC_BUILD)/lc_optee_features.h $(LC_BUILD)/libleancrypto-optee.a: leancrypto
	@test -f '$@'

clean: clean-leancrypto
.PHONY: clean-leancrypto
clean-leancrypto:
	rm -rf -- '$(LC_BUILD)'
