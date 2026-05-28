/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing a device's own storage from within it
 *
 * @defgroup tapi_flash_mtd Raw and MTD storage (tapi_flash_mtd)
 * @ingroup tapi_flash
 * @{
 *
 * Writing a partition of an embedded Linux target from a shell on the
 * target itself: the agent here is the device under test. This is how a
 * running system takes its own update - a new rootfs onto the spare
 * partition, a new kernel onto @c /boot - and how a test flashes without
 * any external programmer.
 *
 * Two kinds of storage, told apart by the device node:
 *
 * - a **block device** or eMMC partition (@c /dev/mmcblk0p3,
 *   @c /dev/sda2): written with @c dd, read back with @c dd, and that
 *   is all it needs;
 * - an **MTD partition** (@c /dev/mtd3), raw NOR or NAND: it must be
 *   erased before it is written, NAND skips bad blocks, and the tools
 *   are @c flash_erase, @c flashcp for NOR, @c nandwrite for NAND.
 *   tapi_flash_mtd_write() picks the right one from the partition's
 *   type in @c /proc/mtd.
 *
 * @code
 * tapi_flash_mtd mtd = tapi_flash_mtd_default;
 * tapi_flash_image image;
 * tapi_flash_result result;
 *
 * mtd.device = "/dev/mtd3";
 * CHECK_RC(tapi_flash_image_init(factory, "/tmp/rootfs.img", NULL, &image));
 * CHECK_RC(tapi_flash_mtd_write(factory, &mtd, &image, &result));
 * @endcode
 *
 * @note The agent is the target, so the image is a path on the target.
 *       Copy it there first, or fetch it there in the update itself.
 */

#ifndef __TSF_TAPI_FLASH_MTD_H__
#define __TSF_TAPI_FLASH_MTD_H__

#include "tapi_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/** What kind of storage a device node is. */
typedef enum tapi_flash_mtd_kind {
    /** Decide from the node: @c /dev/mtd* is MTD, anything else block. */
    TAPI_FLASH_MTD_AUTO = 0,
    /** A block device or eMMC partition; use @c dd. */
    TAPI_FLASH_MTD_BLOCK,
    /** NOR flash; erase then @c flashcp. */
    TAPI_FLASH_MTD_NOR,
    /** NAND flash; erase then @c nandwrite, skipping bad blocks. */
    TAPI_FLASH_MTD_NAND,
} tapi_flash_mtd_kind;

/** A storage partition on the target and how to write it. */
typedef struct tapi_flash_mtd {
    /** Device node, e.g. @c "/dev/mtd3" or @c "/dev/mmcblk0p3". */
    const char *device;
    /** Kind, or TAPI_FLASH_MTD_AUTO to work it out. */
    tapi_flash_mtd_kind kind;
    /** Erase the whole partition before writing, for a block device. */
    bool erase_first;
    /** Byte offset to write at, for @c dd on a block device. */
    uint64_t offset;
    /** Sync and drop caches after writing, so a read back hits storage. */
    bool sync;
} tapi_flash_mtd;

/** Defaults: auto kind, no offset, sync afterwards. */
extern const tapi_flash_mtd tapi_flash_mtd_default;

/** One partition of @c /proc/mtd. */
typedef struct tapi_flash_mtd_part {
    /** Node name, e.g. @c "mtd3". */
    char *name;
    /** Partition label as @c /proc/mtd gives it. */
    char *label;
    /** Size, bytes. */
    uint64_t size;
    /** Erase block size, bytes. */
    uint32_t erasesize;
} tapi_flash_mtd_part;

/**
 * Read the MTD partition table, @c /proc/mtd.
 *
 * @param[in]  factory  Job factory.
 * @param[out] parts    Vector of #tapi_flash_mtd_part to append to,
 *                      initialized with
 *                      @c TE_VEC_INIT(tapi_flash_mtd_part); release it
 *                      with tapi_flash_mtd_parts_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_mtd_parts(tapi_job_factory_t *factory,
                                     te_vec *parts);

/**
 * Parse @c /proc/mtd.
 *
 * @param[in]  text     Contents.
 * @param[out] parts    Vector of #tapi_flash_mtd_part to append to.
 *
 * @return Status code.
 */
extern te_errno tapi_flash_mtd_parts_parse(const char *text, te_vec *parts);

/**
 * Find an MTD partition by its label.
 *
 * @param[in]  parts    Vector of #tapi_flash_mtd_part.
 * @param[in]  label    Label to look for.
 *
 * @return The partition, or @c NULL.
 */
extern const tapi_flash_mtd_part *tapi_flash_mtd_by_label(const te_vec *parts,
                                                          const char *label);

/**
 * Release MTD partitions and the vector holding them.
 *
 * @param parts         Vector of #tapi_flash_mtd_part.
 */
extern void tapi_flash_mtd_parts_free(te_vec *parts);

/**
 * Erase a partition (@c flash_erase for MTD; zero-fill for a block
 * device when asked).
 *
 * @param[in]  factory  Job factory.
 * @param[in]  mtd      Partition.
 * @param[out] result   Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_mtd_erase(tapi_job_factory_t *factory,
                                     const tapi_flash_mtd *mtd,
                                     tapi_flash_result *result);

/**
 * Write an image to a partition, erasing MTD first and choosing the tool
 * by the partition's kind.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  mtd      Partition.
 * @param[in]  image    Image.
 * @param[out] result   Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_mtd_write(tapi_job_factory_t *factory,
                                     const tapi_flash_mtd *mtd,
                                     const tapi_flash_image *image,
                                     tapi_flash_result *result);

/**
 * Read @p size bytes of a partition into a file on the target, with
 * @c dd (block) or @c nanddump/@c dd (MTD).
 *
 * @param[in]  factory  Job factory.
 * @param[in]  mtd      Partition.
 * @param[in]  dest     Path on the target to write to.
 * @param[in]  size     Bytes to read.
 * @param[out] result   Result; release with tapi_flash_result_free().
 *
 * @return Status code.
 */
extern te_errno tapi_flash_mtd_read(tapi_job_factory_t *factory,
                                    const tapi_flash_mtd *mtd,
                                    const char *dest, uint64_t size,
                                    tapi_flash_result *result);

/**
 * Write an image, read the same length back and compare, reporting a
 * mismatch as @c flash.verify.
 *
 * @param[in]  factory  Job factory.
 * @param[in]  mtd      Partition.
 * @param[in]  image    Image.
 * @param[in]  readback Path on the target for the read back.
 * @param[out] issues   Issue list to append to (may be @c NULL).
 * @param[out] matched  Where to save whether it matched (may be @c NULL).
 *
 * @return Status code.
 */
extern te_errno tapi_flash_mtd_write_verify(tapi_job_factory_t *factory,
                                            const tapi_flash_mtd *mtd,
                                            const tapi_flash_image *image,
                                            const char *readback,
                                            tapi_kernel_issues *issues,
                                            bool *matched);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_FLASH_MTD_H__ */

/**@} <!-- END tapi_flash_mtd --> */
