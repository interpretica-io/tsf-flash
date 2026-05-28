/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing Espressif chips
 *
 * Implementation of the esptool wrapper.
 */

#define TE_LGR_USER "TAPI FLASH ESP"

#include "te_config.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"

#include "tapi_flash_esp.h"
#include "tapi_flash_internal.h"

/* See description in tapi_flash_esp.h */
const tapi_flash_esp tapi_flash_esp_default = {
    .port = NULL, .baud = 0, .chip = NULL, .stay_in_bootloader = false,
};

/**
 * esptool is called @c esptool on newer installs and @c esptool.py on
 * older ones; @c command -v settles it and the caller runs the winner
 * through @c sh, so this only needs the name.
 */
#define ESP_LAUNCH \
    "e=esptool; command -v esptool >/dev/null || e=esptool.py; "

/** Append the chip, port, baud and after options common to every call. */
static void
esp_common(const tapi_flash_esp *esp, te_string *script, te_vec *args,
           te_vec *pool)
{
    (void)pool;

    te_string_append(script, ESP_LAUNCH "\"$e\"");
    if (esp->chip != NULL)
    {
        te_string_append(script, " --chip \"$1\"");
        TE_VEC_APPEND(args, esp->chip);
    }
    if (esp->port != NULL)
    {
        size_t idx = te_vec_size(args) + 1;

        te_string_append(script, " --port \"$%zu\"", idx);
        TE_VEC_APPEND(args, esp->port);
    }
    if (esp->baud > 0)
    {
        char *b = te_string_fmt("%u", esp->baud);
        const char *cb = b;
        size_t idx = te_vec_size(args) + 1;

        TE_VEC_APPEND(pool, b);
        te_string_append(script, " --baud \"$%zu\"", idx);
        TE_VEC_APPEND(args, cb);
    }
    if (esp->stay_in_bootloader)
        te_string_append(script, " --after no_reset");
}

/** Run the assembled esptool script and fill a result. */
static te_errno
esp_exec(tapi_job_factory_t *factory, te_string *script, te_vec *args,
         te_vec *pool, tapi_flash_result *result)
{
    struct timeval start;
    struct timeval end;
    int status = -1;
    te_errno rc;

    gettimeofday(&start, NULL);
    rc = tapi_flash_sh(factory, "esptool", script->ptr,
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
        if (!result->ok)
            ERROR("esptool failed (exit %d): %s", status,
                  result->output.ptr != NULL ? result->output.ptr : "");
    }

    te_string_free(script);
    te_vec_free(args);
    te_vec_deep_free(pool);

    return rc;
}

/* See description in tapi_flash_esp.h */
te_errno
tapi_flash_esp_write(tapi_job_factory_t *factory, const tapi_flash_esp *esp,
                     uint64_t offset, const tapi_flash_image *image,
                     tapi_flash_result *result)
{
    te_string script = TE_STRING_INIT;
    te_vec args = TE_VEC_INIT(const char *);
    te_vec pool = TE_VEC_INIT_AUTOPTR(char *);
    char *off = te_string_fmt("0x%" PRIx64, offset);
    const char *coff = off;
    size_t idx;

    tapi_flash_result_init(result);
    esp_common(esp, &script, &args, &pool);

    TE_VEC_APPEND(&pool, off);
    te_string_append(&script, " write_flash");
    idx = te_vec_size(&args) + 1;
    te_string_append(&script, " \"$%zu\"", idx);
    TE_VEC_APPEND(&args, coff);
    idx = te_vec_size(&args) + 1;
    te_string_append(&script, " \"$%zu\"", idx);
    TE_VEC_APPEND(&args, image->path);

    result->bytes = image->size;

    return esp_exec(factory, &script, &args, &pool, result);
}

/* See description in tapi_flash_esp.h */
te_errno
tapi_flash_esp_read(tapi_job_factory_t *factory, const tapi_flash_esp *esp,
                    uint64_t offset, uint64_t size, const char *dest,
                    tapi_flash_result *result)
{
    te_string script = TE_STRING_INIT;
    te_vec args = TE_VEC_INIT(const char *);
    te_vec pool = TE_VEC_INIT_AUTOPTR(char *);
    char *off = te_string_fmt("0x%" PRIx64, offset);
    char *sz = te_string_fmt("0x%" PRIx64, size);
    const char *coff = off;
    const char *csz = sz;
    size_t idx;

    tapi_flash_result_init(result);
    esp_common(esp, &script, &args, &pool);

    TE_VEC_APPEND(&pool, off);
    TE_VEC_APPEND(&pool, sz);
    te_string_append(&script, " read_flash");
    idx = te_vec_size(&args) + 1;
    te_string_append(&script, " \"$%zu\"", idx);
    TE_VEC_APPEND(&args, coff);
    idx = te_vec_size(&args) + 1;
    te_string_append(&script, " \"$%zu\"", idx);
    TE_VEC_APPEND(&args, csz);
    idx = te_vec_size(&args) + 1;
    te_string_append(&script, " \"$%zu\"", idx);
    TE_VEC_APPEND(&args, dest);

    result->bytes = size;

    return esp_exec(factory, &script, &args, &pool, result);
}

/* See description in tapi_flash_esp.h */
te_errno
tapi_flash_esp_erase(tapi_job_factory_t *factory, const tapi_flash_esp *esp,
                     tapi_flash_result *result)
{
    te_string script = TE_STRING_INIT;
    te_vec args = TE_VEC_INIT(const char *);
    te_vec pool = TE_VEC_INIT_AUTOPTR(char *);

    tapi_flash_result_init(result);
    esp_common(esp, &script, &args, &pool);
    te_string_append(&script, " erase_flash");

    return esp_exec(factory, &script, &args, &pool, result);
}

/* See description in tapi_flash_esp.h */
te_errno
tapi_flash_esp_chip_id(tapi_job_factory_t *factory, const tapi_flash_esp *esp,
                       te_string *info, bool *answered)
{
    te_string script = TE_STRING_INIT;
    te_vec args = TE_VEC_INIT(const char *);
    te_vec pool = TE_VEC_INIT_AUTOPTR(char *);
    tapi_flash_result result;
    te_errno rc;

    if (answered != NULL)
        *answered = false;

    tapi_flash_result_init(&result);
    esp_common(esp, &script, &args, &pool);
    te_string_append(&script, " flash_id");

    rc = esp_exec(factory, &script, &args, &pool, &result);
    if (rc == 0)
    {
        if (result.output.ptr != NULL)
            te_string_append(info, "%s", result.output.ptr);
        if (answered != NULL)
            *answered = result.ok;
    }
    tapi_flash_result_free(&result);

    return rc;
}

/** Context for the verify read-back. */
typedef struct esp_read_ctx {
    tapi_job_factory_t *factory;
    const tapi_flash_esp *esp;
    uint64_t offset;
} esp_read_ctx;

static te_errno
esp_read_region(void *ctx, const char *dest, uint64_t size)
{
    esp_read_ctx *c = ctx;
    tapi_flash_result result;
    te_errno rc;

    tapi_flash_result_init(&result);
    rc = tapi_flash_esp_read(c->factory, c->esp, c->offset, size, dest,
                             &result);
    if (rc == 0 && !result.ok)
        rc = TE_RC(TE_TAPI, TE_EFAIL);
    tapi_flash_result_free(&result);

    return rc;
}

/* See description in tapi_flash_esp.h */
te_errno
tapi_flash_esp_write_verify(tapi_job_factory_t *factory,
                            const tapi_flash_esp *esp, uint64_t offset,
                            const tapi_flash_image *image,
                            const char *readback, tapi_kernel_issues *issues,
                            bool *matched)
{
    tapi_flash_esp write_esp = *esp;
    tapi_flash_result result;
    esp_read_ctx ctx = { .factory = factory, .esp = esp, .offset = offset };
    te_errno rc;

    /* Keep the bootloader up so the read back can run over serial. */
    write_esp.stay_in_bootloader = true;
    tapi_flash_result_init(&result);
    rc = tapi_flash_esp_write(factory, &write_esp, offset, image, &result);
    if (rc == 0 && !result.ok)
    {
        if (issues != NULL)
            tapi_kernel_issues_add(issues, "flash.write", image->path,
                                   "esptool could not write the image "
                                   "(exit %d)", result.status);
        if (matched != NULL)
            *matched = false;
        tapi_flash_result_free(&result);
        return rc;
    }
    tapi_flash_result_free(&result);
    if (rc != 0)
        return rc;

    return tapi_flash_verify(factory, image, readback, esp_read_region, &ctx,
                             issues, matched);
}
