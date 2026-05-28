/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing a device's own storage from within it
 *
 * Implementation of the block and MTD writers.
 */

#define TE_LGR_USER "TAPI FLASH MTD"

#include "te_config.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"

#include "tapi_flash_mtd.h"
#include "tapi_flash_internal.h"

/* See description in tapi_flash_mtd.h */
const tapi_flash_mtd tapi_flash_mtd_default = {
    .device = NULL, .kind = TAPI_FLASH_MTD_AUTO, .erase_first = false,
    .offset = 0, .sync = true,
};

/** Release one MTD partition. */
static void
mtd_part_destroy(tapi_flash_mtd_part *p)
{
    free(p->name);
    free(p->label);
}

/* See description in tapi_flash_mtd.h */
void
tapi_flash_mtd_parts_free(te_vec *parts)
{
    tapi_flash_mtd_part *p;

    TE_VEC_FOREACH(parts, p)
        mtd_part_destroy(p);

    te_vec_free(parts);
}

/* See description in tapi_flash_mtd.h */
te_errno
tapi_flash_mtd_parts_parse(const char *text, te_vec *parts)
{
    char *copy = TE_STRDUP(text);
    char *save = NULL;
    char *line;

    /* dev:    size   erasesize  name */
    /* mtd0: 00100000 00010000 "u-boot" */
    for (line = strtok_r(copy, "\n", &save); line != NULL;
         line = strtok_r(NULL, "\n", &save))
    {
        tapi_flash_mtd_part p;
        char *colon = strchr(line, ':');
        char *q1;
        char *q2;
        unsigned long long size;
        unsigned long erasesize;

        if (colon == NULL || strncmp(line, "mtd", 3) != 0)
            continue;

        *colon = '\0';
        if (sscanf(colon + 1, "%llx %lx", &size, &erasesize) != 2)
            continue;

        q1 = strchr(colon + 1, '"');
        q2 = (q1 != NULL) ? strchr(q1 + 1, '"') : NULL;

        memset(&p, 0, sizeof(p));
        p.name = TE_STRDUP(line);
        p.size = size;
        p.erasesize = (uint32_t)erasesize;
        if (q1 != NULL && q2 != NULL)
        {
            p.label = TE_ALLOC(q2 - q1);
            memcpy(p.label, q1 + 1, q2 - q1 - 1);
        }
        else
        {
            p.label = TE_STRDUP("");
        }
        TE_VEC_APPEND(parts, p);
    }

    free(copy);

    return 0;
}

/* See description in tapi_flash_mtd.h */
te_errno
tapi_flash_mtd_parts(tapi_job_factory_t *factory, te_vec *parts)
{
    const char *args[] = { "/proc/mtd" };
    te_string out = TE_STRING_INIT;
    int status;
    te_errno rc;

    rc = tapi_flash_run(factory, "cat", "cat", args, 1,
                        TAPI_FLASH_QUERY_TIMEOUT_MS, &out, NULL, &status);
    if (rc == 0 && status != 0)
    {
        ERROR("The target has no /proc/mtd");
        rc = TE_RC(TE_TAPI, TE_ENOENT);
    }
    if (rc == 0)
        rc = tapi_flash_mtd_parts_parse(out.ptr != NULL ? out.ptr : "", parts);

    te_string_free(&out);

    return rc;
}

/* See description in tapi_flash_mtd.h */
const tapi_flash_mtd_part *
tapi_flash_mtd_by_label(const te_vec *parts, const char *label)
{
    const tapi_flash_mtd_part *p;

    TE_VEC_FOREACH(parts, p)
    {
        if (p->label != NULL && strcmp(p->label, label) == 0)
            return p;
    }

    return NULL;
}

/** Work out the kind of a device from its node and, for MTD, /proc/mtd. */
static te_errno
mtd_resolve_kind(tapi_job_factory_t *factory, const tapi_flash_mtd *mtd,
                 tapi_flash_mtd_kind *kind)
{
    const char *node;
    te_string flags = TE_STRING_INIT;
    const char *args[3];
    int status;
    te_errno rc = 0;

    if (mtd->kind != TAPI_FLASH_MTD_AUTO)
    {
        *kind = mtd->kind;
        return 0;
    }

    node = strrchr(mtd->device, '/');
    node = (node != NULL) ? node + 1 : mtd->device;

    if (strncmp(node, "mtd", 3) != 0)
    {
        *kind = TAPI_FLASH_MTD_BLOCK;
        return 0;
    }

    /* MTD: NAND or NOR? Ask sysfs for the type. */
    args[0] = "-c";
    args[1] = "cat \"/sys/class/mtd/${1##*/}/type\" 2>/dev/null || echo nor";
    args[2] = mtd->device;
    rc = tapi_flash_run(factory, "sh", "sh", args, 3,
                        TAPI_FLASH_QUERY_TIMEOUT_MS, &flags, NULL, &status);
    if (rc == 0)
    {
        if (flags.ptr != NULL && strstr(flags.ptr, "nand") != NULL)
            *kind = TAPI_FLASH_MTD_NAND;
        else
            *kind = TAPI_FLASH_MTD_NOR;
    }

    te_string_free(&flags);

    return rc;
}

/** Run a shell script and fill a result from it. */
static te_errno
mtd_exec(tapi_job_factory_t *factory, const char *name, const char *script,
         const char **args, size_t n_args, tapi_flash_result *result)
{
    struct timeval start;
    struct timeval end;
    int status = -1;
    te_errno rc;

    gettimeofday(&start, NULL);
    rc = tapi_flash_sh(factory, name, script, args, n_args,
                       TAPI_FLASH_TIMEOUT_MS, &result->output,
                       &result->output, &status);
    gettimeofday(&end, NULL);

    if (rc == 0)
    {
        result->status = status;
        result->ok = (status == 0);
        result->seconds = (end.tv_sec - start.tv_sec) +
                          (end.tv_usec - start.tv_usec) / 1e6;
        if (!result->ok)
            ERROR("%s failed (exit %d): %s", name, status,
                  result->output.ptr != NULL ? result->output.ptr : "");
    }

    return rc;
}

/* See description in tapi_flash_mtd.h */
te_errno
tapi_flash_mtd_erase(tapi_job_factory_t *factory, const tapi_flash_mtd *mtd,
                     tapi_flash_result *result)
{
    tapi_flash_mtd_kind kind;
    const char *args[1] = { mtd->device };
    te_errno rc;

    tapi_flash_result_init(result);
    rc = mtd_resolve_kind(factory, mtd, &kind);
    if (rc != 0)
        return rc;

    if (kind == TAPI_FLASH_MTD_NOR || kind == TAPI_FLASH_MTD_NAND)
        return mtd_exec(factory, "flash_erase",
                        "flash_erase \"$1\" 0 0", args, 1, result);

    /* Block device: zero it if asked, otherwise nothing to erase. */
    return mtd_exec(factory, "blkdiscard",
                    "blkdiscard \"$1\" 2>/dev/null || "
                    "dd if=/dev/zero of=\"$1\" bs=1M 2>/dev/null; "
                    "sync; exit 0", args, 1, result);
}

/* See description in tapi_flash_mtd.h */
te_errno
tapi_flash_mtd_write(tapi_job_factory_t *factory, const tapi_flash_mtd *mtd,
                     const tapi_flash_image *image, tapi_flash_result *result)
{
    tapi_flash_mtd_kind kind;
    const char *args[3];
    te_errno rc;

    tapi_flash_result_init(result);
    rc = mtd_resolve_kind(factory, mtd, &kind);
    if (rc != 0)
        return rc;

    args[0] = image->path;
    args[1] = mtd->device;

    switch (kind)
    {
        case TAPI_FLASH_MTD_NOR:
        {
            te_string script = TE_STRING_INIT;

            /* flashcp erases what it writes; -v gives a progress line. */
            te_string_append(&script, "flashcp -v \"$1\" \"$2\"");
            if (mtd->sync)
                te_string_append(&script, " && sync");
            rc = mtd_exec(factory, "flashcp", script.ptr, args, 2, result);
            te_string_free(&script);
            break;
        }

        case TAPI_FLASH_MTD_NAND:
        {
            te_string script = TE_STRING_INIT;

            te_string_append(&script,
                             "flash_erase \"$2\" 0 0 && "
                             "nandwrite -p \"$2\" \"$1\"");
            if (mtd->sync)
                te_string_append(&script, "; sync");
            rc = mtd_exec(factory, "nandwrite", script.ptr, args, 2, result);
            te_string_free(&script);
            break;
        }

        case TAPI_FLASH_MTD_BLOCK:
        default:
        {
            te_string script = TE_STRING_INIT;
            char *off = te_string_fmt("%" PRIu64, mtd->offset);

            args[2] = off;
            te_string_append(&script,
                             "dd if=\"$1\" of=\"$2\" bs=1M seek=$3 "
                             "oflag=seek_bytes conv=fsync 2>&1");
            if (mtd->sync)
                te_string_append(&script, "; sync; "
                                 "echo 3 > /proc/sys/vm/drop_caches "
                                 "2>/dev/null; exit 0");
            rc = mtd_exec(factory, "dd", script.ptr, args, 3, result);
            free(off);
            te_string_free(&script);
            break;
        }
    }

    result->bytes = image->size;

    return rc;
}

/* See description in tapi_flash_mtd.h */
te_errno
tapi_flash_mtd_read(tapi_job_factory_t *factory, const tapi_flash_mtd *mtd,
                    const char *dest, uint64_t size, tapi_flash_result *result)
{
    tapi_flash_mtd_kind kind;
    te_string script = TE_STRING_INIT;
    char *off;
    char *count;
    const char *args[4];
    te_errno rc;

    tapi_flash_result_init(result);
    rc = mtd_resolve_kind(factory, mtd, &kind);
    if (rc != 0)
        return rc;

    off = te_string_fmt("%" PRIu64, mtd->offset);
    count = te_string_fmt("%" PRIu64, size);
    args[0] = mtd->device;
    args[1] = dest;
    args[2] = count;
    args[3] = off;

    if (kind == TAPI_FLASH_MTD_NAND)
    {
        /* nanddump skips bad blocks the way nandwrite did; -l bytes. */
        te_string_append(&script,
                         "if command -v nanddump >/dev/null; then "
                         "nanddump -o -l \"$3\" -f \"$2\" \"$1\"; "
                         "else dd if=\"$1\" of=\"$2\" bs=1 count=\"$3\" "
                         "skip=\"$4\" 2>&1; fi");
    }
    else
    {
        te_string_append(&script,
                         "dd if=\"$1\" of=\"$2\" bs=1 count=\"$3\" "
                         "skip=\"$4\" iflag=skip_bytes 2>&1");
    }

    rc = mtd_exec(factory, "read", script.ptr, args, 4, result);
    result->bytes = size;

    free(off);
    free(count);
    te_string_free(&script);

    return rc;
}

/** Context for the verify read-back. */
typedef struct mtd_read_ctx {
    tapi_job_factory_t *factory;
    const tapi_flash_mtd *mtd;
} mtd_read_ctx;

static te_errno
mtd_read_region(void *ctx, const char *dest, uint64_t size)
{
    mtd_read_ctx *c = ctx;
    tapi_flash_result result;
    te_errno rc;

    tapi_flash_result_init(&result);
    rc = tapi_flash_mtd_read(c->factory, c->mtd, dest, size, &result);
    if (rc == 0 && !result.ok)
        rc = TE_RC(TE_TAPI, TE_EFAIL);
    tapi_flash_result_free(&result);

    return rc;
}

/* See description in tapi_flash_mtd.h */
te_errno
tapi_flash_mtd_write_verify(tapi_job_factory_t *factory,
                            const tapi_flash_mtd *mtd,
                            const tapi_flash_image *image,
                            const char *readback, tapi_kernel_issues *issues,
                            bool *matched)
{
    tapi_flash_result result;
    mtd_read_ctx ctx = { .factory = factory, .mtd = mtd };
    te_errno rc;

    tapi_flash_result_init(&result);
    rc = tapi_flash_mtd_write(factory, mtd, image, &result);
    if (rc == 0 && !result.ok)
    {
        if (issues != NULL)
            tapi_kernel_issues_add(issues, "flash.write", mtd->device,
                                   "writing %s failed (exit %d)",
                                   image->path, result.status);
        if (matched != NULL)
            *matched = false;
        tapi_flash_result_free(&result);
        return rc;
    }
    tapi_flash_result_free(&result);
    if (rc != 0)
        return rc;

    return tapi_flash_verify(factory, image, readback, mtd_read_region, &ctx,
                             issues, matched);
}
