cmake_minimum_required(VERSION 3.22)
project(optee_pqc VERSION 0.2.0 LANGUAGES C)

option(PQC_BUILD_TA "Build the ARM32 or ARM64 OP-TEE TA using its own devkit" OFF)
option(PQC_BUILD_CLIENT "Build the Linux libteec client" OFF)
option(PQC_LEANCRYPTO_ASM "Enable Leancrypto AArch64 assembly/NEON (requires TA float support)" OFF)
set(PQC_TA_DEV_KIT "" CACHE PATH "Matching OP-TEE export-ta_arm32 or export-ta_arm64 directory")
set(PQC_TA_CROSS_COMPILE "" CACHE STRING "TA compiler prefix (empty selects from devkit architecture)")
set(PQC_TEEC_ROOT "" CACHE PATH "libteec installation prefix for the Linux target")
set(PQC_TA_INSTALL_DIR "lib/optee_armtz" CACHE STRING "TA install directory (absolute or relative to prefix)")

include(GNUInstallDirs)

if(PQC_BUILD_TA)
    if(NOT EXISTS "${PQC_TA_DEV_KIT}/mk/ta_dev_kit.mk")
        message(FATAL_ERROR "Set PQC_TA_DEV_KIT to the matching export-ta_arm32 or export-ta_arm64 directory")
    endif()
    file(STRINGS "${PQC_TA_DEV_KIT}/mk/conf.mk" PQC_TA_SM REGEX "^sm[ \t]*:=[ \t]*ta_arm(32|64)[ \t]*$")
    if(PQC_TA_SM MATCHES "ta_arm64")
        set(PQC_TA_DEFAULT_PREFIX "aarch64-linux-gnu-")
    elseif(PQC_TA_SM MATCHES "ta_arm32")
        if(PQC_LEANCRYPTO_ASM)
            message(FATAL_ERROR "PQC_LEANCRYPTO_ASM requires an ARM64 TA devkit")
        endif()
        set(PQC_TA_DEFAULT_PREFIX "arm-linux-gnueabihf-")
    else()
        message(FATAL_ERROR "Unsupported TA architecture in devkit mk/conf.mk")
    endif()
    set(PQC_TA_PREFIX "${PQC_TA_CROSS_COMPILE}")
    if(NOT PQC_TA_PREFIX)
        set(PQC_TA_PREFIX "${PQC_TA_DEFAULT_PREFIX}")
    endif()
    find_program(PQC_MAKE make REQUIRED)
    add_custom_target(pqc-ta ALL
        COMMAND "${PQC_MAKE}" -C "${CMAKE_CURRENT_SOURCE_DIR}/ta"
            "TA_DEV_KIT_DIR=${PQC_TA_DEV_KIT}"
            "CROSS_COMPILE=${PQC_TA_PREFIX}"
            "PQC_LEANCRYPTO_ASM=$<BOOL:${PQC_LEANCRYPTO_ASM}>"
            "O=${CMAKE_CURRENT_BINARY_DIR}/ta"
        BYPRODUCTS
            "${CMAKE_CURRENT_BINARY_DIR}/ta/leancrypto/libleancrypto-optee.a"
            "${CMAKE_CURRENT_BINARY_DIR}/ta/7b1e6f40-9c32-4da8-a567-71930c4e82b6.ta"
        VERBATIM)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ta/7b1e6f40-9c32-4da8-a567-71930c4e82b6.ta"
        DESTINATION "${PQC_TA_INSTALL_DIR}")
endif()

if(PQC_BUILD_CLIENT)
    find_path(PQC_TEEC_INCLUDE tee_client_api.h HINTS "${PQC_TEEC_ROOT}/include" REQUIRED)
    find_library(PQC_TEEC_LIBRARY teec HINTS "${PQC_TEEC_ROOT}/lib" REQUIRED)
    add_library(pqc_tz_client_lib STATIC client/pqc_tz_client.c)
    target_include_directories(pqc_tz_client_lib PUBLIC include client "${PQC_TEEC_INCLUDE}")
    target_link_libraries(pqc_tz_client_lib PUBLIC "${PQC_TEEC_LIBRARY}")
    target_compile_options(pqc_tz_client_lib PRIVATE -Wall -Wextra -Werror)
    add_executable(pqc_tz_client client/main.c)
    target_link_libraries(pqc_tz_client PRIVATE pqc_tz_client_lib)
    target_compile_options(pqc_tz_client PRIVATE -Wall -Wextra -Werror)
    install(TARGETS pqc_tz_client pqc_tz_client_lib
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    install(FILES client/pqc_tz_client.h include/pqc_algorithms.h include/pqc_protocol.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/optee-pqc")
endif()
