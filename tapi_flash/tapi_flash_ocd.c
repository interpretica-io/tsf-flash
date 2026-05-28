/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing over JTAG or SWD
 *
 * Implementation of the OpenOCD wrapper.
 */

#define TE_LGR_USER "TAPI FLASH OCD"

#include "te_config.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"

#include "tapi_flash_ocd.h"
#include "tapi_flash_internal.h"

/* See description in tapi_flash_ocd.h */
const tapi_flash_ocd tapi_flash_ocd_default = {
    .configs = NULL, .n_configs = 0, .pre_commands = NULL,
    .n_pre_commands = 0, .search_dir = NULL,
};

/**
 * Build the openocd argument vector: search dir, config files, pre
 * commands, then each of @p commands as its own @c -c. A quoted string
 * in a command must already be quoted for Tcl by the caller.
 */
static void
ocd_args(const tapi_flash_ocd *ocd, const char **commands, size_t n_commands,
         te_vec *args)
{
    const char *opt;
    size_t i;

    if (ocd->search_dir != NULL)
    {
        opt = "-s";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, ocd->search_dir);
    }
    for (i = 0; i < ocd->n_configs; i++)
    {
        opt = "-f";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, ocd->configs[i]);
    }
    for (i = 0; i < ocd->n_pre_commands; i++)
    {
        opt = "-c";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, ocd->pre_commands[i]);
    }
    for (i = 0; i < n_commands; i++)
    {
        opt = "-c";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, commands[i]);
    }
}

/** Run openocd and fill a result. */
static te_errno
ocd_exec(tapi_job_factory_t *factory, te_vec *args, tapi_flash_result *result)
{
    struct timeval start;
    struct timeval end;
    int status = -1;
    te_errno rc;

    gettimeofday(&start, NULL);
    rc = tapi_flash_run(factory, "openocd", "openocd",
                        (const char **)te_vec_get_mutable(args, 0),
                        te_vec_size(args), TAPI_FLASH_TIMEOUT_MS,
                        &result->output, &result->output, &status);
    gettimeofday(&end, NULL);

    if (rc == 0)
    {
        result->status = status;
        /*
         * OpenOCD can exit 0 having printed an error, so the output is
         * checked too: a failed verify or a lost target says so in words.
         */
        result->ok = (status == 0) && result->output.ptr != NULL &&
                     strstr(result->output.ptr, "Error:") == NULL;
        result->seconds = (end.tv_sec - start.tv_sec) +
                          (end.tv_usec - start.tv_usec) / 1e6;
        if (!result->ok)
            ERROR("openocd failed (exit %d): %s", status,
                  result->output.ptr != NULL ? result->output.ptr : "");
    }

    te_vec_free(args);

    return rc;
}

/* See description in tapi_flash_ocd.h */
te_errno
tapi_flash_ocd_run(tapi_job_factory_t *factory, const tapi_flash_ocd *ocd,
                   const char **commands, size_t n_commands,
                   tapi_flash_result *result)
{
    te_vec args = TE_VEC_INIT(const char *);
    const char **all;
    const char *shutdown = "shutdown";
    size_t i;
    te_errno rc;

    tapi_flash_result_init(result);

    /* Always finish with shutdown, or openocd waits for a connection. */
    all = TE_ALLOC((n_commands + 1) * sizeof(*all));
    for (i = 0; i < n_commands; i++)
        all[i] = commands[i];
    all[n_commands] = shutdown;

    ocd_args(ocd, all, n_commands + 1, &args);
    free(all);

    rc = ocd_exec(factory, &args, result);

    return rc;
}

/* See description in tapi_flash_ocd.h */
te_errno
tapi_flash_ocd_program(tapi_job_factory_t *factory, const tapi_flash_ocd *ocd,
                       const tapi_flash_image *image, uint64_t address,
                       bool verify, tapi_flash_result *result)
{
    te_string cmd = TE_STRING_INIT;
    bool is_raw = image->format != NULL &&
                  strcmp(image->format, "bin") == 0;
    const char *commands[1];
    te_errno rc;

    /*
     * "program <file> [verify] [reset]" - a bin needs the address, an
     * elf or hex carries its own. The file path is wrapped in braces so
     * Tcl does not split it on spaces.
     */
    te_string_append(&cmd, "program {%s}", image->path);
    if (verify)
        te_string_append(&cmd, " verify");
    if (is_raw)
        te_string_append(&cmd, " 0x%" PRIx64, address);
    te_string_append(&cmd, "; reset run");

    commands[0] = cmd.ptr;
    rc = tapi_flash_ocd_run(factory, ocd, commands, 1, result);
    result->bytes = image->size;

    te_string_free(&cmd);

    return rc;
}

/* See description in tapi_flash_ocd.h */
te_errno
tapi_flash_ocd_dump(tapi_job_factory_t *factory, const tapi_flash_ocd *ocd,
                    uint64_t address, uint64_t size, const char *dest,
                    tapi_flash_result *result)
{
    te_string cmd = TE_STRING_INIT;
    const char *commands[3];
    te_errno rc;

    commands[0] = "init";
    commands[1] = "reset halt";
    te_string_append(&cmd, "dump_image {%s} 0x%" PRIx64 " %" PRIu64,
                     dest, address, size);
    commands[2] = cmd.ptr;

    rc = tapi_flash_ocd_run(factory, ocd, commands, 3, result);
    result->bytes = size;

    te_string_free(&cmd);

    return rc;
}

/* See description in tapi_flash_ocd.h */
te_errno
tapi_flash_ocd_reset(tapi_job_factory_t *factory, const tapi_flash_ocd *ocd,
                     tapi_flash_result *result)
{
    const char *commands[] = { "init", "reset run" };

    return tapi_flash_ocd_run(factory, ocd, commands, TE_ARRAY_LEN(commands),
                              result);
}

/** Context for the verify read-back. */
typedef struct ocd_read_ctx {
    tapi_job_factory_t *factory;
    const tapi_flash_ocd *ocd;
    uint64_t address;
} ocd_read_ctx;

static te_errno
ocd_read_region(void *ctx, const char *dest, uint64_t size)
{
    ocd_read_ctx *c = ctx;
    tapi_flash_result result;
    te_errno rc;

    tapi_flash_result_init(&result);
    rc = tapi_flash_ocd_dump(c->factory, c->ocd, c->address, size, dest,
                             &result);
    if (rc == 0 && !result.ok)
        rc = TE_RC(TE_TAPI, TE_EFAIL);
    tapi_flash_result_free(&result);

    return rc;
}

/* See description in tapi_flash_ocd.h */
te_errno
tapi_flash_ocd_program_verify(tapi_job_factory_t *factory,
                              const tapi_flash_ocd *ocd,
                              const tapi_flash_image *image, uint64_t address,
                              const char *readback, tapi_kernel_issues *issues,
                              bool *matched)
{
    tapi_flash_result result;
    ocd_read_ctx ctx = { .factory = factory, .ocd = ocd, .address = address };
    te_errno rc;

    tapi_flash_result_init(&result);
    rc = tapi_flash_ocd_program(factory, ocd, image, address, false, &result);
    if (rc == 0 && !result.ok)
    {
        if (issues != NULL)
            tapi_kernel_issues_add(issues, "flash.write", image->path,
                                   "openocd could not program the image "
                                   "(exit %d)", result.status);
        if (matched != NULL)
            *matched = false;
        tapi_flash_result_free(&result);
        return rc;
    }
    tapi_flash_result_free(&result);
    if (rc != 0)
        return rc;

    return tapi_flash_verify(factory, image, readback, ocd_read_region, &ctx,
                             issues, matched);
}
