/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing over JTAG or SWD
 *
 * @defgroup tapi_flash_ocd OpenOCD (tapi_flash_ocd)
 * @ingroup tapi_flash
 * @{
 *
 * Programming a microcontroller over a debug probe - JTAG or SWD - with
 * @c openocd on the host the probe is attached to. This reaches a chip
 * that has no USB bootloader and no running Linux: the way to flash a
 * bare board on the bench, and the way in when a bad image left it
 * unable to boot.
 *
 * OpenOCD is driven by configuration files - one for the probe (the
 * @c interface), one for the board or chip (the @c target) - and a
 * sequence of commands. This wrapper runs a batch: it starts OpenOCD
 * with those configs, sends the commands and exits.
 *
 * @code
 * tapi_flash_ocd ocd = tapi_flash_ocd_default;
 * static const char *cfg[] = { "interface/stlink.cfg", "target/stm32f4x.cfg" };
 * tapi_flash_image image;
 * tapi_flash_result result;
 *
 * ocd.configs = cfg; ocd.n_configs = 2;
 * CHECK_RC(tapi_flash_image_init(factory, "/tmp/fw.elf", "elf", &image));
 * CHECK_RC(tapi_flash_ocd_program(factory, &ocd, &image, 0, true, &result));
 * @endcode
 */

#ifndef __TSF_TAPI_FLASH_OCD_H__
#define __TSF_TAPI_FLASH_OCD_H__

#include "tapi_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/** How to reach a chip with OpenOCD. */
typedef struct tapi_flash_ocd {
    /** Config files (@c -f), probe first then target. */
    const char **configs;
    /** Number of @a configs. */
    size_t n_configs;
    /** Commands to run before the configs (@c -c), e.g. an adapter speed. */
    const char **pre_commands;
    /** Number of @a pre_commands. */
    size_t n_pre_commands;
    /** Search path for configs (@c -s), or @c NULL. */
    const char *search_dir;
} tapi_flash_ocd;

/** Defaults: no extra commands, OpenOCD's own config search path. */
extern const tapi_flash_ocd tapi_flash_ocd_default;

/**
 * Program an image and optionally verify and reset, with the OpenOCD
 * @c program command.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  ocd      How to reach the chip.
 * @param[in]  image    Image; @c elf and @c hex carry their own load
 *                      address and ignore @p address.
 * @param[in]  address  Load address for a raw @c bin, e.g. @c 0x08000000.
 * @param[in]  verify   Have OpenOCD verify after programming.
 * @param[out] result   Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_ocd_program(tapi_job_factory_t *factory,
                                       const tapi_flash_ocd *ocd,
                                       const tapi_flash_image *image,
                                       uint64_t address, bool verify,
                                       tapi_flash_result *result);

/**
 * Read @p size bytes of flash from @p address into a file on the agent,
 * with @c dump_image.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  ocd      How to reach the chip.
 * @param[in]  address  Start address.
 * @param[in]  size     Bytes to read.
 * @param[in]  dest     Path on the agent to write to.
 * @param[out] result   Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_ocd_dump(tapi_job_factory_t *factory,
                                    const tapi_flash_ocd *ocd,
                                    uint64_t address, uint64_t size,
                                    const char *dest,
                                    tapi_flash_result *result);

/**
 * Reset and run the target (@c "reset run").
 *
 * @param[in]  factory  Job factory.
 * @param[in]  ocd      How to reach the chip.
 * @param[out] result   Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_ocd_reset(tapi_job_factory_t *factory,
                                     const tapi_flash_ocd *ocd,
                                     tapi_flash_result *result);

/**
 * Run an arbitrary batch of OpenOCD commands, in the configured session,
 * and capture what they printed. The batch always ends with
 * @c shutdown.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  ocd          How to reach the chip.
 * @param[in]  commands     Commands to run in order.
 * @param[in]  n_commands   Number of @p commands.
 * @param[out] result       Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_ocd_run(tapi_job_factory_t *factory,
                                   const tapi_flash_ocd *ocd,
                                   const char **commands, size_t n_commands,
                                   tapi_flash_result *result);

/**
 * Program an image, dump the same length back from @p address and
 * compare, reporting a mismatch as @c flash.verify. Use this when the
 * transport's own verify is not trusted, or to get the mismatch as an
 * issue rather than an exit status.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  ocd      How to reach the chip.
 * @param[in]  image    Image.
 * @param[in]  address  Address the image was written to and is read from.
 * @param[in]  readback Path on the agent for the read back.
 * @param[out] issues   Issue list to append to (may be @c NULL).
 * @param[out] matched  Where to save whether it matched (may be @c NULL).
 *
 * @return Status code.
 */
extern te_errno tapi_flash_ocd_program_verify(tapi_job_factory_t *factory,
                                              const tapi_flash_ocd *ocd,
                                              const tapi_flash_image *image,
                                              uint64_t address,
                                              const char *readback,
                                              tapi_kernel_issues *issues,
                                              bool *matched);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_FLASH_OCD_H__ */

/**@} <!-- END tapi_flash_ocd --> */
