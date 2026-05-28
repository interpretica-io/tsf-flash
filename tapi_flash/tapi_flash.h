/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing a device under test
 *
 * @defgroup tapi_flash Flashing a device under test (tapi_flash)
 * @{
 *
 * Writing firmware onto a device from a Test Agent, whichever way the
 * device takes it, and proving it landed.
 *
 * A test suite flashes for two reasons: to put the build under test on
 * the device before the run, and to make flashing itself the thing
 * under test - that a bad image is refused, that an interrupted write
 * recovers, that the device comes back. Both want the same handful of
 * operations - erase, write, read back, verify, reset - over tools that
 * share nothing else:
 *
 * - @ref tapi_flash_dfu - USB DFU, with @c dfu-util;
 * - @ref tapi_flash_fastboot - Android bootloader, with @c fastboot;
 * - @ref tapi_flash_mtd - a raw block or MTD device on an embedded
 *   Linux target itself, with @c dd, @c flashcp, @c nandwrite and
 *   @c flash_erase;
 * - @ref tapi_flash_rom - SPI and BIOS flash chips, with @c flashrom;
 * - @ref tapi_flash_esp - Espressif chips, with @c esptool;
 * - @ref tapi_flash_ocd - anything on JTAG or SWD, with @c openocd.
 *
 * This group holds what they have in common: the image being written,
 * how a device is named to a tool, the result of an operation, finding
 * out which tools the agent has, and waiting for a device to go away and
 * come back across a reset.
 *
 * @section tapi_flash_where Where the tool runs
 *
 * The tool runs on the agent behind the job factory. For an external
 * programmer - a DFU device on a USB port, a JTAG probe, an SPI clip -
 * that is the host the device is wired to, and the agent may be the
 * engine host itself. For @ref tapi_flash_mtd the agent is the device
 * under test: it flashes its own MTD partitions. The image is a path on
 * whichever host that is; put it there first with @c tapi_file_copy_ta()
 * of TE.
 *
 * @section tapi_flash_verify Proving it landed
 *
 * A tool that returns success has written what it was given; it has not
 * shown that the flash holds it. Where the transport can read back -
 * DFU upload, @c flashrom @c -r, @c dd from the device, @c esptool
 * @c read_flash - tapi_flash_verify() reads the region and compares its
 * SHA-256 with the image's. That is the check a flashing test should
 * fail on, not the exit status of the writer.
 *
 * @section tapi_flash_issues Reporting
 *
 * The operations return a @ref tapi_flash_result and a status code, for
 * a test that flashes as a step and fails outright if it cannot. The
 * checks that belong to a flashing test - a verify that did not match,
 * a device that did not come back - also append to the
 * @ref tapi_kernel_issue list of tsf-kernel, so a flashing suite reports
 * the same way as the rest.
 *
 * @note Everything here needs an RPC job factory, for the reason given
 *       in tsf-devtool: reading what a tool printed needs output
 *       channels, which only that factory implements.
 *
 * @note Flashing writes to hardware and can leave a device that will
 *       not boot. Everything here points at the device the suite's own
 *       configuration names; that is the engagement it belongs to.
 */

#ifndef __TSF_TAPI_FLASH_H__
#define __TSF_TAPI_FLASH_H__

#include <stdint.h>

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "te_vector.h"
#include "tapi_job.h"

#include "tapi_kernel_issue.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default timeout of a flashing operation, ms. Erasing a chip is slow. */
#define TAPI_FLASH_TIMEOUT_MS       300000

/** Timeout of a quick query - a tool version, a device list, ms. */
#define TAPI_FLASH_QUERY_TIMEOUT_MS 30000

/** An image to write, or one read back from a device. */
typedef struct tapi_flash_image {
    /** Path of the file on the host the tool runs on. */
    char *path;
    /** Size in bytes; 0 until it has been taken. */
    uint64_t size;
    /** SHA-256 in hex; empty until it has been taken. */
    char digest[65];
    /**
     * Format, if the tool needs telling: @c "bin", @c "hex", @c "elf",
     * @c "uf2". @c NULL lets the tool decide, which most do by
     * extension.
     */
    char *format;
} tapi_flash_image;

/**
 * Describe an image by its path and take its size and digest from the
 * agent.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  path     Path of the image on the host the tool runs on.
 * @param[in]  format   Format, or @c NULL (see tapi_flash_image).
 * @param[out] image    Image; release it with tapi_flash_image_free().
 *
 * @return Status code.
 * @retval TE_ENOENT    There is no such file on the agent.
 */
extern te_errno tapi_flash_image_init(tapi_job_factory_t *factory,
                                      const char *path, const char *format,
                                      tapi_flash_image *image);

/**
 * Release an image description.
 *
 * @param image         Image.
 */
extern void tapi_flash_image_free(tapi_flash_image *image);

/** What one flashing operation did. */
typedef struct tapi_flash_result {
    /** @c true if the tool exited successfully. */
    bool ok;
    /** Exit status of the tool, or @c -1 if it did not exit normally. */
    int status;
    /** Bytes written or read, if the tool reported a count; else 0. */
    uint64_t bytes;
    /** Seconds the operation took. */
    double seconds;
    /** Everything the tool printed, kept for the log and for verdicts. */
    te_string output;
} tapi_flash_result;

/**
 * Prepare an empty result.
 *
 * @param result        Result.
 */
extern void tapi_flash_result_init(tapi_flash_result *result);

/**
 * Write a result into the log.
 *
 * @param result        Result.
 * @param what          What the operation was, for the message.
 */
extern void tapi_flash_result_log(const tapi_flash_result *result,
                                  const char *what);

/**
 * Release a result.
 *
 * @param result        Result.
 */
extern void tapi_flash_result_free(tapi_flash_result *result);

/** Which flashing tools an agent has. */
typedef struct tapi_flash_tools {
    bool dfu_util;      /**< @c dfu-util */
    bool fastboot;      /**< @c fastboot */
    bool flashrom;      /**< @c flashrom */
    bool esptool;       /**< @c esptool or @c esptool.py */
    bool openocd;       /**< @c openocd */
    bool flashcp;       /**< @c flashcp (mtd-utils) */
    bool nandwrite;     /**< @c nandwrite (mtd-utils) */
    bool flash_erase;   /**< @c flash_erase (mtd-utils) */
    bool dd;            /**< @c dd */
} tapi_flash_tools;

/**
 * Find out which flashing tools the agent has.
 *
 * @param[in]  factory  Job factory.
 * @param[out] tools    Where to save the answer.
 *
 * @return Status code.
 */
extern te_errno tapi_flash_tools_probe(tapi_job_factory_t *factory,
                                       tapi_flash_tools *tools);

/**
 * Compare an image with a region read back from a device, and report a
 * mismatch as @c flash.verify.
 *
 * The region is read with @p readback - a function that writes the
 * device's contents to a file on the agent, e.g. tapi_flash_dfu_read()
 * or tapi_flash_mtd_read() with the arguments bound - and its first
 * @a size bytes are hashed and compared with the image. A device whose
 * read back is larger because it padded to an erase block still matches.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  image        Image that was written.
 * @param[in]  readback     Path on the agent to read the device into.
 * @param[in]  read_region  Callback that fills @p readback from the
 *                          device; its @c void* is @p ctx.
 * @param[in]  ctx          Context for @p read_region.
 * @param[out] issues       Issue list to append to (may be @c NULL).
 * @param[out] matched      Where to save whether it matched (may be
 *                          @c NULL).
 *
 * @return Status code.
 */
extern te_errno tapi_flash_verify(tapi_job_factory_t *factory,
                                  const tapi_flash_image *image,
                                  const char *readback,
                                  te_errno (*read_region)(void *ctx,
                                                          const char *dest,
                                                          uint64_t size),
                                  void *ctx,
                                  tapi_kernel_issues *issues,
                                  bool *matched);

/**
 * Wait for a device to disappear and come back, as it does across a
 * reset into a new firmware.
 *
 * @p present is called repeatedly - it answers whether the device can be
 * seen now, e.g. a USB id is listed or a path exists - until it has said
 * "gone" and then "back", or the timeout runs out. A device that never
 * left (a reset too quick to catch) is not a failure; one that left and
 * did not return is @c flash.no-return.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  subject      What the device is, for the issue subject.
 * @param[in]  present      Callback: is the device visible now? Its
 *                          @c void* is @p ctx; sets its @c bool* out.
 * @param[in]  ctx          Context for @p present.
 * @param[in]  timeout_ms   How long to wait for the round trip, ms.
 * @param[in]  poll_ms      How often to look, ms.
 * @param[out] issues       Issue list to append to (may be @c NULL).
 * @param[out] returned     Where to save whether it came back (may be
 *                          @c NULL).
 *
 * @return Status code.
 */
extern te_errno tapi_flash_wait_reappear(tapi_job_factory_t *factory,
                                         const char *subject,
                                         te_errno (*present)(void *ctx,
                                                             bool *visible),
                                         void *ctx, int timeout_ms,
                                         int poll_ms,
                                         tapi_kernel_issues *issues,
                                         bool *returned);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_FLASH_H__ */

/**@} <!-- END tapi_flash --> */
