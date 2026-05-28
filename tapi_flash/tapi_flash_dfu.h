/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing over USB DFU
 *
 * @defgroup tapi_flash_dfu USB DFU (tapi_flash_dfu)
 * @ingroup tapi_flash
 * @{
 *
 * Writing and reading firmware over the USB Device Firmware Upgrade
 * class, with @c dfu-util on the host the device is plugged into.
 *
 * A DFU device is named by its vendor and product id, and, when it has
 * more than one, by the DFU interface (@c "alt") - internal flash,
 * option bytes, external SPI. Many chips also take a target address on
 * the command line (@c dfuse), which is where a partial image goes.
 *
 * @code
 * tapi_flash_dfu dfu = tapi_flash_dfu_default;
 * tapi_flash_image image;
 * tapi_flash_result result;
 *
 * dfu.vid = 0x0483; dfu.pid = 0xdf11; dfu.alt = 0;
 * dfu.address = 0x08000000; dfu.leave = true;
 * CHECK_RC(tapi_flash_image_init(factory, "/tmp/fw.bin", "bin", &image));
 * CHECK_RC(tapi_flash_dfu_write(factory, &dfu, &image, &result));
 * tapi_flash_result_log(&result, "DFU write");
 * @endcode
 */

#ifndef __TSF_TAPI_FLASH_DFU_H__
#define __TSF_TAPI_FLASH_DFU_H__

#include "tapi_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/** A DFU device and where on it to write. */
typedef struct tapi_flash_dfu {
    /** USB vendor id. */
    uint16_t vid;
    /** USB product id. */
    uint16_t pid;
    /** DFU interface (@c -a); -1 to leave it to dfu-util. */
    int alt;
    /** Path of a specific device on the bus (@c -p), or @c NULL. */
    const char *bus_path;
    /** Serial of a specific device (@c -S), or @c NULL. */
    const char *serial;
    /** dfuse start address (@c -s); 0 to leave it off. */
    uint64_t address;
    /**
     * Bytes to upload for a read back (@c -s @a address:@a length); 0
     * to read to the end. Only for tapi_flash_dfu_read().
     */
    uint64_t length;
    /** Ask the device to leave DFU and run the firmware afterwards. */
    bool leave;
} tapi_flash_dfu;

/** Defaults: no alt, no address, do not leave DFU. */
extern const tapi_flash_dfu tapi_flash_dfu_default;

/**
 * Download an image to a DFU device.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  dfu      Device.
 * @param[in]  image    Image.
 * @param[out] result   Result; release it with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_dfu_write(tapi_job_factory_t *factory,
                                     const tapi_flash_dfu *dfu,
                                     const tapi_flash_image *image,
                                     tapi_flash_result *result);

/**
 * Upload from a DFU device into a file on the agent.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  dfu      Device; its @a address and @a length say what to
 *                      read.
 * @param[in]  dest     Path on the agent to write the read back to.
 * @param[out] result   Result; release it with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_dfu_read(tapi_job_factory_t *factory,
                                    const tapi_flash_dfu *dfu,
                                    const char *dest,
                                    tapi_flash_result *result);

/**
 * Check whether a DFU device with the given ids is on the bus, by
 * listing (@c dfu-util @c -l).
 *
 * @param[in]  factory  Job factory.
 * @param[in]  dfu      Device; only @a vid and @a pid are used.
 * @param[out] present  Where to save the answer.
 *
 * @return Status code.
 */
extern te_errno tapi_flash_dfu_present(tapi_job_factory_t *factory,
                                       const tapi_flash_dfu *dfu,
                                       bool *present);

/**
 * Write an image, then read it back and compare, reporting a mismatch
 * as @c flash.verify.
 *
 * The device must stay in DFU for the read back, so this sets @a leave
 * off for the write regardless of what @p dfu says.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  dfu      Device.
 * @param[in]  image    Image.
 * @param[in]  readback Path on the agent for the read back.
 * @param[out] issues   Issue list to append to (may be @c NULL).
 * @param[out] matched  Where to save whether it matched (may be @c NULL).
 *
 * @return Status code.
 */
extern te_errno tapi_flash_dfu_write_verify(tapi_job_factory_t *factory,
                                            const tapi_flash_dfu *dfu,
                                            const tapi_flash_image *image,
                                            const char *readback,
                                            tapi_kernel_issues *issues,
                                            bool *matched);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_FLASH_DFU_H__ */

/**@} <!-- END tapi_flash_dfu --> */
