# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Interpretica Unipessoal Lda
#
# External TE library (see TE_EXT_REPO): engine-side TAPIs for flashing a
# device under test from a Test Agent - USB DFU, Android fastboot, a
# target's own raw and MTD storage, SPI/BIOS flash chips, Espressif chips
# and anything on JTAG or SWD - with verification and reappear checks.
# Built as a shared library on the engine platform and as a static
# library when agents or agent apps are built.

if get_option('engine')
    build_lib_shared = true
endif
build_lib_static = not get_option('engine') \
    or get_option('agents').split().length() != 0 \
    or get_option('agent-apps').split().length() != 0
install_lib = install_dev

headers += files(
    'tapi_flash.h',
    'tapi_flash_dfu.h',
    'tapi_flash_esp.h',
    'tapi_flash_fastboot.h',
    'tapi_flash_mtd.h',
    'tapi_flash_ocd.h',
    'tapi_flash_rom.h',
)

# tapi_flash_internal.h is internal to this library and is not installed.
sources += files(
    'tapi_flash.c',
    'tapi_flash_dfu.c',
    'tapi_flash_esp.c',
    'tapi_flash_fastboot.c',
    'tapi_flash_mtd.c',
    'tapi_flash_ocd.c',
    'tapi_flash_rom.c',
)

te_libs += [
    'tapi',
    'tapi_job',
    'tapi_devtool',
    'tapi_kernel',
]
