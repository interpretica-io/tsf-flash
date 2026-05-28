/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing SPI and BIOS flash chips
 *
 * @defgroup tapi_flash_rom SPI and BIOS flash (tapi_flash_rom)
 * @ingroup tapi_flash
 * @{
 *
 * Reading and writing a flash chip directly, over one of the many
 * programmers @c flashrom drives - an FT2232 SPI dongle, a Raspberry Pi
 * GPIO header, the target's own internal programmer - on the host the
 * programmer is attached to.
 *
 * A programmer is named the way flashrom names it (@c -p): the driver
 * and its parameters, e.g. @c "ft2232_spi:type=232H",
 * @c "linux_spi:dev=/dev/spidev0.0,spispeed=8000". flashrom writes the
 * whole chip and verifies as it goes; a region layout can restrict it to
 * one named region.
 *
 * @code
 * tapi_flash_rom rom = tapi_flash_rom_default;
 * tapi_flash_image image;
 * tapi_flash_result result;
 *
 * rom.programmer = "linux_spi:dev=/dev/spidev0.0,spispeed=8000";
 * CHECK_RC(tapi_flash_image_init(factory, "/tmp/bios.rom", NULL, &image));
 * CHECK_RC(tapi_flash_rom_write(factory, &rom, &image, &result));
 * @endcode
 */

#ifndef __TSF_TAPI_FLASH_ROM_H__
#define __TSF_TAPI_FLASH_ROM_H__

#include "tapi_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/** A flash chip behind a flashrom programmer. */
typedef struct tapi_flash_rom {
    /** Programmer spec (@c -p), e.g. @c "ft2232_spi:type=232H". */
    const char *programmer;
    /** Chip name (@c -c), or @c NULL to let flashrom detect it. */
    const char *chip;
    /** Layout file on the agent (@c --layout), or @c NULL. */
    const char *layout;
    /** Region within the layout to act on (@c --image), or @c NULL. */
    const char *region;
    /** Do not verify after writing (@c -N); flashrom verifies by default. */
    bool no_verify;
} tapi_flash_rom;

/** Defaults: detect the chip, whole chip, verify after writing. */
extern const tapi_flash_rom tapi_flash_rom_default;

/**
 * Write an image to the chip. flashrom reads, erases only what differs,
 * writes and verifies unless @a no_verify is set.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  rom      Chip.
 * @param[in]  image    Image.
 * @param[out] result   Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_rom_write(tapi_job_factory_t *factory,
                                     const tapi_flash_rom *rom,
                                     const tapi_flash_image *image,
                                     tapi_flash_result *result);

/**
 * Read the chip into a file on the agent.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  rom      Chip.
 * @param[in]  dest     Path on the agent to write to.
 * @param[out] result   Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_rom_read(tapi_job_factory_t *factory,
                                    const tapi_flash_rom *rom,
                                    const char *dest,
                                    tapi_flash_result *result);

/**
 * Erase the chip.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  rom      Chip.
 * @param[out] result   Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_rom_erase(tapi_job_factory_t *factory,
                                     const tapi_flash_rom *rom,
                                     tapi_flash_result *result);

/**
 * Probe the chip: detect it and report what flashrom found (its name and
 * size), without touching its contents.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  rom      Chip.
 * @param[out] found    String to append what flashrom detected to.
 * @param[out] detected Where to save whether a chip was found (may be
 *                      @c NULL).
 *
 * @return Status code.
 */
extern te_errno tapi_flash_rom_probe(tapi_job_factory_t *factory,
                                     const tapi_flash_rom *rom,
                                     te_string *found, bool *detected);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_FLASH_ROM_H__ */

/**@} <!-- END tapi_flash_rom --> */
