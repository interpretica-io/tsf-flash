/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing over USB DFU
 *
 * Implementation of the dfu-util wrapper.
 */

#define TE_LGR_USER "TAPI FLASH DFU"

#include "te_config.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"

#include "tapi_flash_dfu.h"
#include "tapi_flash_internal.h"

/* See description in tapi_flash_dfu.h */
const tapi_flash_dfu tapi_flash_dfu_default = {
    .vid = 0, .pid = 0, .alt = -1, .bus_path = NULL, .serial = NULL,
    .address = 0, .length = 0, .leave = false,
};

/** Append the id and selector options common to every dfu-util call. */
static void
dfu_common_args(const tapi_flash_dfu *dfu, te_vec *args, te_vec *pool)
{
    char *id = te_string_fmt("%04x:%04x", dfu->vid, dfu->pid);
    const char *opt;

    TE_VEC_APPEND(pool, id);
    opt = "-d";
    TE_VEC_APPEND(args, opt);
    {
        const char *cid = id;
        TE_VEC_APPEND(args, cid);
    }

    if (dfu->alt >= 0)
    {
        char *alt = te_string_fmt("%d", dfu->alt);
        const char *calt = alt;

        TE_VEC_APPEND(pool, alt);
        opt = "-a";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, calt);
    }
    if (dfu->bus_path != NULL)
    {
        opt = "-p";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, dfu->bus_path);
    }
    if (dfu->serial != NULL)
    {
        opt = "-S";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, dfu->serial);
    }
}

/** Parse "Downloaded/Received N bytes" out of dfu-util output. */
static uint64_t
dfu_bytes(const char *output)
{
    static const char *marks[] = { "Downloaded ", "Received ", "Download ",
                                   "Upload " };
    size_t i;

    for (i = 0; i < TE_ARRAY_LEN(marks); i++)
    {
        const char *p = strstr(output, marks[i]);

        while (p != NULL)
        {
            const char *digits = p + strlen(marks[i]);

            if (*digits >= '0' && *digits <= '9')
            {
                char *end;
                uint64_t n = strtoull(digits, &end, 10);

                while (*end == ' ')
                    end++;
                if (strncmp(end, "bytes", 5) == 0)
                    return n;
            }
            p = strstr(p + 1, marks[i]);
        }
    }

    return 0;
}

/** Run dfu-util with the assembled arguments and fill a result. */
static te_errno
dfu_exec(tapi_job_factory_t *factory, te_vec *args, te_vec *pool,
         tapi_flash_result *result)
{
    struct timeval start;
    struct timeval end;
    int status = -1;
    te_errno rc;

    gettimeofday(&start, NULL);
    rc = tapi_flash_run(factory, "dfu-util", "dfu-util",
                        (const char **)te_vec_get_mutable(args, 0),
                        te_vec_size(args), TAPI_FLASH_TIMEOUT_MS,
                        &result->output, &result->output, &status);
    gettimeofday(&end, NULL);

    if (rc == 0)
    {
        result->status = status;
        result->ok = (status == 0);
        result->seconds = (end.tv_sec - start.tv_sec) +
                          (end.tv_usec - start.tv_usec) / 1e6;
        if (result->output.ptr != NULL)
            result->bytes = dfu_bytes(result->output.ptr);
        if (!result->ok)
            ERROR("dfu-util failed (exit %d): %s", status,
                  result->output.ptr != NULL ? result->output.ptr : "");
    }

    te_vec_free(args);
    te_vec_deep_free(pool);

    return rc;
}

/** Format a dfuse address[:length] value into the pool. */
static const char *
dfu_dfuse(const tapi_flash_dfu *dfu, te_vec *pool, bool with_length,
          bool leave)
{
    te_string s = TE_STRING_INIT;
    char *owned;

    te_string_append(&s, "0x%" PRIx64, dfu->address);
    if (with_length && dfu->length > 0)
        te_string_append(&s, ":%" PRIu64, dfu->length);
    if (leave)
        te_string_append(&s, ":leave");
    owned = s.ptr;
    TE_VEC_APPEND(pool, owned);

    return owned;
}

/* See description in tapi_flash_dfu.h */
te_errno
tapi_flash_dfu_write(tapi_job_factory_t *factory, const tapi_flash_dfu *dfu,
                     const tapi_flash_image *image, tapi_flash_result *result)
{
    te_vec args = TE_VEC_INIT(const char *);
    te_vec pool = TE_VEC_INIT_AUTOPTR(char *);
    const char *opt;

    tapi_flash_result_init(result);
    dfu_common_args(dfu, &args, &pool);

    if (dfu->address > 0)
    {
        opt = "-s";
        TE_VEC_APPEND(&args, opt);
        opt = dfu_dfuse(dfu, &pool, false, dfu->leave);
        TE_VEC_APPEND(&args, opt);
    }
    else if (dfu->leave)
    {
        opt = "-R";
        TE_VEC_APPEND(&args, opt);
    }

    opt = "-D";
    TE_VEC_APPEND(&args, opt);
    TE_VEC_APPEND(&args, image->path);

    return dfu_exec(factory, &args, &pool, result);
}

/* See description in tapi_flash_dfu.h */
te_errno
tapi_flash_dfu_read(tapi_job_factory_t *factory, const tapi_flash_dfu *dfu,
                    const char *dest, tapi_flash_result *result)
{
    te_vec args = TE_VEC_INIT(const char *);
    te_vec pool = TE_VEC_INIT_AUTOPTR(char *);
    const char *opt;

    tapi_flash_result_init(result);
    dfu_common_args(dfu, &args, &pool);

    if (dfu->address > 0 || dfu->length > 0)
    {
        opt = "-s";
        TE_VEC_APPEND(&args, opt);
        opt = dfu_dfuse(dfu, &pool, true, false);
        TE_VEC_APPEND(&args, opt);
    }

    opt = "-U";
    TE_VEC_APPEND(&args, opt);
    TE_VEC_APPEND(&args, dest);

    return dfu_exec(factory, &args, &pool, result);
}

/* See description in tapi_flash_dfu.h */
te_errno
tapi_flash_dfu_present(tapi_job_factory_t *factory, const tapi_flash_dfu *dfu,
                       bool *present)
{
    const char *args[] = { "-l" };
    te_string out = TE_STRING_INIT;
    char needle[16];
    te_errno rc;

    *present = false;

    rc = tapi_flash_run(factory, "dfu-util", "dfu-util", args, 1,
                        TAPI_FLASH_QUERY_TIMEOUT_MS, &out, &out, NULL);
    if (rc == 0 && out.ptr != NULL)
    {
        snprintf(needle, sizeof(needle), "%04x:%04x", dfu->vid, dfu->pid);
        *present = (strstr(out.ptr, needle) != NULL);
    }

    te_string_free(&out);

    return rc;
}

/** Context for the verify read-back callback. */
typedef struct dfu_read_ctx {
    tapi_job_factory_t *factory;
    const tapi_flash_dfu *dfu;
} dfu_read_ctx;

static te_errno
dfu_read_region(void *ctx, const char *dest, uint64_t size)
{
    dfu_read_ctx *c = ctx;
    tapi_flash_dfu dfu = *c->dfu;
    tapi_flash_result result;
    te_errno rc;

    dfu.length = size;
    dfu.leave = false;
    tapi_flash_result_init(&result);
    rc = tapi_flash_dfu_read(c->factory, &dfu, dest, &result);
    if (rc == 0 && !result.ok)
        rc = TE_RC(TE_TAPI, TE_EFAIL);
    tapi_flash_result_free(&result);

    return rc;
}

/* See description in tapi_flash_dfu.h */
te_errno
tapi_flash_dfu_write_verify(tapi_job_factory_t *factory,
                            const tapi_flash_dfu *dfu,
                            const tapi_flash_image *image,
                            const char *readback, tapi_kernel_issues *issues,
                            bool *matched)
{
    tapi_flash_dfu write_dfu = *dfu;
    tapi_flash_result result;
    dfu_read_ctx ctx = { .factory = factory, .dfu = dfu };
    te_errno rc;

    /* Stay in DFU so the read back can run. */
    write_dfu.leave = false;
    tapi_flash_result_init(&result);
    rc = tapi_flash_dfu_write(factory, &write_dfu, image, &result);
    if (rc == 0 && !result.ok)
    {
        if (issues != NULL)
            tapi_kernel_issues_add(issues, "flash.write", image->path,
                                   "dfu-util could not write the image "
                                   "(exit %d)", result.status);
        if (matched != NULL)
            *matched = false;
        tapi_flash_result_free(&result);
        return rc;
    }
    tapi_flash_result_free(&result);
    if (rc != 0)
        return rc;

    return tapi_flash_verify(factory, image, readback, dfu_read_region, &ctx,
                             issues, matched);
}
