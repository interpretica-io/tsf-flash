/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing SPI and BIOS flash chips
 *
 * Implementation of the flashrom wrapper.
 */

#define TE_LGR_USER "TAPI FLASH ROM"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"

#include "tapi_flash_rom.h"
#include "tapi_flash_internal.h"

/* See description in tapi_flash_rom.h */
const tapi_flash_rom tapi_flash_rom_default = {
    .programmer = NULL, .chip = NULL, .layout = NULL, .region = NULL,
    .no_verify = false,
};

/** Append the programmer, chip and layout options common to every call. */
static void
rom_common_args(const tapi_flash_rom *rom, te_vec *args)
{
    const char *opt;

    if (rom->programmer != NULL)
    {
        opt = "-p";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, rom->programmer);
    }
    if (rom->chip != NULL)
    {
        opt = "-c";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, rom->chip);
    }
    if (rom->layout != NULL)
    {
        opt = "--layout";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, rom->layout);
    }
    if (rom->region != NULL)
    {
        opt = "--image";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, rom->region);
    }
}

/** Run flashrom with the assembled arguments and fill a result. */
static te_errno
rom_exec(tapi_job_factory_t *factory, te_vec *args, tapi_flash_result *result)
{
    struct timeval start;
    struct timeval end;
    int status = -1;
    te_errno rc;

    gettimeofday(&start, NULL);
    rc = tapi_flash_run(factory, "flashrom", "flashrom",
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
            ERROR("flashrom failed (exit %d): %s", status,
                  result->output.ptr != NULL ? result->output.ptr : "");
    }

    te_vec_free(args);

    return rc;
}

/* See description in tapi_flash_rom.h */
te_errno
tapi_flash_rom_write(tapi_job_factory_t *factory, const tapi_flash_rom *rom,
                     const tapi_flash_image *image, tapi_flash_result *result)
{
    te_vec args = TE_VEC_INIT(const char *);
    const char *opt;

    tapi_flash_result_init(result);
    rom_common_args(rom, &args);
    opt = "-w";
    TE_VEC_APPEND(&args, opt);
    TE_VEC_APPEND(&args, image->path);
    if (rom->no_verify)
    {
        opt = "-N";
        TE_VEC_APPEND(&args, opt);
    }

    return rom_exec(factory, &args, result);
}

/* See description in tapi_flash_rom.h */
te_errno
tapi_flash_rom_read(tapi_job_factory_t *factory, const tapi_flash_rom *rom,
                    const char *dest, tapi_flash_result *result)
{
    te_vec args = TE_VEC_INIT(const char *);
    const char *opt;

    tapi_flash_result_init(result);
    rom_common_args(rom, &args);
    opt = "-r";
    TE_VEC_APPEND(&args, opt);
    TE_VEC_APPEND(&args, dest);

    return rom_exec(factory, &args, result);
}

/* See description in tapi_flash_rom.h */
te_errno
tapi_flash_rom_erase(tapi_job_factory_t *factory, const tapi_flash_rom *rom,
                     tapi_flash_result *result)
{
    te_vec args = TE_VEC_INIT(const char *);
    const char *opt;

    tapi_flash_result_init(result);
    rom_common_args(rom, &args);
    opt = "-E";
    TE_VEC_APPEND(&args, opt);

    return rom_exec(factory, &args, result);
}

/* See description in tapi_flash_rom.h */
te_errno
tapi_flash_rom_probe(tapi_job_factory_t *factory, const tapi_flash_rom *rom,
                     te_string *found, bool *detected)
{
    te_vec args = TE_VEC_INIT(const char *);
    te_string out = TE_STRING_INIT;
    int status;
    te_errno rc;

    if (detected != NULL)
        *detected = false;

    rom_common_args(rom, &args);

    rc = tapi_flash_run(factory, "flashrom", "flashrom",
                        (const char **)te_vec_get_mutable(&args, 0),
                        te_vec_size(&args), TAPI_FLASH_QUERY_TIMEOUT_MS,
                        &out, &out, &status);
    te_vec_free(&args);
    if (rc != 0)
        goto out;

    if (out.ptr != NULL)
    {
        te_string_append(found, "%s", out.ptr);
        if (detected != NULL)
            *detected = (status == 0) &&
                        (strstr(out.ptr, "Found") != NULL);
    }

out:
    te_string_free(&out);

    return rc;
}
