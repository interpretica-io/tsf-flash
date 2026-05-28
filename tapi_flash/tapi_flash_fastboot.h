/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing over the Android bootloader
 *
 * @defgroup tapi_flash_fastboot Android fastboot (tapi_flash_fastboot)
 * @ingroup tapi_flash
 * @{
 *
 * Writing partitions of a device that is in its bootloader, with
 * @c fastboot on the host it is plugged into.
 *
 * A device is named by its fastboot serial, or left out when only one
 * is attached. An image goes to a named partition - @c boot, @c system,
 * @c vbmeta - and the device can be erased, rebooted, and asked for its
 * variables (@c getvar).
 *
 * @code
 * tapi_flash_fastboot fb = tapi_flash_fastboot_default;
 * tapi_flash_image image;
 * tapi_flash_result result;
 *
 * fb.serial = "0123456789ABCDEF";
 * CHECK_RC(tapi_flash_image_init(factory, "/tmp/boot.img", NULL, &image));
 * CHECK_RC(tapi_flash_fastboot_flash(factory, &fb, "boot", &image, &result));
 * @endcode
 */

#ifndef __TSF_TAPI_FLASH_FASTBOOT_H__
#define __TSF_TAPI_FLASH_FASTBOOT_H__

#include "tapi_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/** A fastboot device. */
typedef struct tapi_flash_fastboot {
    /** Device serial (@c -s), or @c NULL for the only one attached. */
    const char *serial;
} tapi_flash_fastboot;

/**
 * Defaults: the only device attached. Note that @c "fastboot flash" does
 * not reset the device; reset it with tapi_flash_fastboot_reboot().
 */
extern const tapi_flash_fastboot tapi_flash_fastboot_default;

/**
 * Flash an image to a partition.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  fb           Device.
 * @param[in]  partition    Partition name, e.g. @c "boot".
 * @param[in]  image        Image.
 * @param[out] result       Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_fastboot_flash(tapi_job_factory_t *factory,
                                          const tapi_flash_fastboot *fb,
                                          const char *partition,
                                          const tapi_flash_image *image,
                                          tapi_flash_result *result);

/**
 * Erase a partition.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  fb           Device.
 * @param[in]  partition    Partition name.
 * @param[out] result       Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_fastboot_erase(tapi_job_factory_t *factory,
                                          const tapi_flash_fastboot *fb,
                                          const char *partition,
                                          tapi_flash_result *result);

/**
 * Reboot the device: into the system (@p target @c NULL), or into
 * @c "bootloader", @c "fastboot" or @c "recovery".
 *
 * @param[in]  factory  Job factory.
 * @param[in]  fb       Device.
 * @param[in]  target   Where to reboot to, or @c NULL for the system.
 * @param[out] result   Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_fastboot_reboot(tapi_job_factory_t *factory,
                                           const tapi_flash_fastboot *fb,
                                           const char *target,
                                           tapi_flash_result *result);

/**
 * Read a fastboot variable (@c getvar), e.g. @c "product",
 * @c "current-slot".
 *
 * @param[in]  factory  Job factory.
 * @param[in]  fb       Device.
 * @param[in]  name     Variable name, or @c "all".
 * @param[out] value    String to append the value to.
 *
 * @return Status code.
 */
extern te_errno tapi_flash_fastboot_getvar(tapi_job_factory_t *factory,
                                           const tapi_flash_fastboot *fb,
                                           const char *name, te_string *value);

/**
 * Check whether a device is in fastboot, by listing (@c fastboot
 * @c devices).
 *
 * @param[in]  factory  Job factory.
 * @param[in]  fb       Device; @a serial narrows the check, @c NULL
 *                      accepts any.
 * @param[out] present  Where to save the answer.
 *
 * @return Status code.
 */
extern te_errno tapi_flash_fastboot_present(tapi_job_factory_t *factory,
                                            const tapi_flash_fastboot *fb,
                                            bool *present);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_FLASH_FASTBOOT_H__ */

/**@} <!-- END tapi_flash_fastboot --> */
