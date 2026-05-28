/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing Espressif chips
 *
 * @defgroup tapi_flash_esp Espressif esptool (tapi_flash_esp)
 * @ingroup tapi_flash
 * @{
 *
 * Writing and reading the SPI flash of an ESP8266 or ESP32 over its
 * serial bootloader, with @c esptool on the host it is wired to.
 *
 * A chip is named by its serial port and, if it matters, its baud rate;
 * an image goes to a flash offset - @c 0x0 for the bootloader,
 * @c 0x10000 for the app, wherever the partition table puts it.
 *
 * @code
 * tapi_flash_esp esp = tapi_flash_esp_default;
 * tapi_flash_image image;
 * tapi_flash_result result;
 *
 * esp.port = "/dev/ttyUSB0"; esp.baud = 460800;
 * CHECK_RC(tapi_flash_image_init(factory, "/tmp/app.bin", NULL, &image));
 * CHECK_RC(tapi_flash_esp_write(factory, &esp, 0x10000, &image, &result));
 * @endcode
 */

#ifndef __TSF_TAPI_FLASH_ESP_H__
#define __TSF_TAPI_FLASH_ESP_H__

#include "tapi_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/** An Espressif chip on a serial port. */
typedef struct tapi_flash_esp {
    /** Serial port (@c --port), e.g. @c "/dev/ttyUSB0". */
    const char *port;
    /** Baud rate (@c --baud); 0 for esptool's default. */
    unsigned int baud;
    /** Chip type (@c --chip), e.g. @c "esp32", or @c NULL to detect. */
    const char *chip;
    /** Leave the chip in the bootloader afterwards (@c --after no_reset). */
    bool stay_in_bootloader;
} tapi_flash_esp;

/** Defaults: detect the chip, default baud, reset to run afterwards. */
extern const tapi_flash_esp tapi_flash_esp_default;

/**
 * Write an image at a flash offset (@c write_flash).
 *
 * @param[in]  factory  Job factory.
 * @param[in]  esp      Chip.
 * @param[in]  offset   Flash offset, e.g. @c 0x10000.
 * @param[in]  image    Image.
 * @param[out] result   Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_esp_write(tapi_job_factory_t *factory,
                                     const tapi_flash_esp *esp,
                                     uint64_t offset,
                                     const tapi_flash_image *image,
                                     tapi_flash_result *result);

/**
 * Read @p size bytes from a flash offset into a file on the agent
 * (@c read_flash).
 *
 * @param[in]  factory  Job factory.
 * @param[in]  esp      Chip.
 * @param[in]  offset   Flash offset.
 * @param[in]  size     Bytes to read.
 * @param[in]  dest     Path on the agent to write to.
 * @param[out] result   Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_esp_read(tapi_job_factory_t *factory,
                                    const tapi_flash_esp *esp,
                                    uint64_t offset, uint64_t size,
                                    const char *dest,
                                    tapi_flash_result *result);

/**
 * Erase the whole flash (@c erase_flash).
 *
 * @param[in]  factory  Job factory.
 * @param[in]  esp      Chip.
 * @param[out] result   Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_esp_erase(tapi_job_factory_t *factory,
                                     const tapi_flash_esp *esp,
                                     tapi_flash_result *result);

/**
 * Read the chip id (@c chip_id / @c flash_id), which also proves the
 * bootloader answers.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  esp      Chip.
 * @param[out] info     String to append what esptool reported to.
 * @param[out] answered Where to save whether the chip answered (may be
 *                      @c NULL).
 *
 * @return Status code.
 */
extern te_errno tapi_flash_esp_chip_id(tapi_job_factory_t *factory,
                                       const tapi_flash_esp *esp,
                                       te_string *info, bool *answered);

/**
 * Write an image, read the same length back from the same offset and
 * compare, reporting a mismatch as @c flash.verify.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  esp      Chip.
 * @param[in]  offset   Flash offset.
 * @param[in]  image    Image.
 * @param[in]  readback Path on the agent for the read back.
 * @param[out] issues   Issue list to append to (may be @c NULL).
 * @param[out] matched  Where to save whether it matched (may be @c NULL).
 *
 * @return Status code.
 */
extern te_errno tapi_flash_esp_write_verify(tapi_job_factory_t *factory,
                                            const tapi_flash_esp *esp,
                                            uint64_t offset,
                                            const tapi_flash_image *image,
                                            const char *readback,
                                            tapi_kernel_issues *issues,
                                            bool *matched);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_FLASH_ESP_H__ */

/**@} <!-- END tapi_flash_esp --> */
