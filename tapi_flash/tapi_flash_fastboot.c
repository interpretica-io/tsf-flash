/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing over the Android bootloader
 *
 * Implementation of the fastboot wrapper.
 */

#define TE_LGR_USER "TAPI FLASH FASTBOOT"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "te_str.h"
#include "te_string.h"

#include "tapi_flash_fastboot.h"
#include "tapi_flash_internal.h"

/* See description in tapi_flash_fastboot.h */
const tapi_flash_fastboot tapi_flash_fastboot_default = {
    .serial = NULL,
};

/** Append the serial selector, if any. */
static void
fastboot_serial(const tapi_flash_fastboot *fb, te_vec *args)
{
    const char *opt;

    if (fb->serial != NULL)
    {
        opt = "-s";
        TE_VEC_APPEND(args, opt);
        TE_VEC_APPEND(args, fb->serial);
    }
}

/** Run fastboot with the assembled arguments and fill a result. */
static te_errno
fastboot_exec(tapi_job_factory_t *factory, te_vec *args,
              tapi_flash_result *result)
{
    struct timeval start;
    struct timeval end;
    int status = -1;
    te_errno rc;

    gettimeofday(&start, NULL);
    rc = tapi_flash_run(factory, "fastboot", "fastboot",
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
            ERROR("fastboot failed (exit %d): %s", status,
                  result->output.ptr != NULL ? result->output.ptr : "");
    }

    te_vec_free(args);

    return rc;
}

/* See description in tapi_flash_fastboot.h */
te_errno
tapi_flash_fastboot_flash(tapi_job_factory_t *factory,
                          const tapi_flash_fastboot *fb,
                          const char *partition,
                          const tapi_flash_image *image,
                          tapi_flash_result *result)
{
    te_vec args = TE_VEC_INIT(const char *);
    const char *opt;

    tapi_flash_result_init(result);
    fastboot_serial(fb, &args);
    opt = "flash";
    TE_VEC_APPEND(&args, opt);
    TE_VEC_APPEND(&args, partition);
    TE_VEC_APPEND(&args, image->path);

    return fastboot_exec(factory, &args, result);
}

/* See description in tapi_flash_fastboot.h */
te_errno
tapi_flash_fastboot_erase(tapi_job_factory_t *factory,
                          const tapi_flash_fastboot *fb,
                          const char *partition, tapi_flash_result *result)
{
    te_vec args = TE_VEC_INIT(const char *);
    const char *opt;

    tapi_flash_result_init(result);
    fastboot_serial(fb, &args);
    opt = "erase";
    TE_VEC_APPEND(&args, opt);
    TE_VEC_APPEND(&args, partition);

    return fastboot_exec(factory, &args, result);
}

/* See description in tapi_flash_fastboot.h */
te_errno
tapi_flash_fastboot_reboot(tapi_job_factory_t *factory,
                           const tapi_flash_fastboot *fb, const char *target,
                           tapi_flash_result *result)
{
    te_vec args = TE_VEC_INIT(const char *);
    const char *opt;

    tapi_flash_result_init(result);
    fastboot_serial(fb, &args);
    opt = "reboot";
    TE_VEC_APPEND(&args, opt);
    if (target != NULL)
        TE_VEC_APPEND(&args, target);

    return fastboot_exec(factory, &args, result);
}

/* See description in tapi_flash_fastboot.h */
te_errno
tapi_flash_fastboot_getvar(tapi_job_factory_t *factory,
                           const tapi_flash_fastboot *fb, const char *name,
                           te_string *value)
{
    te_vec args = TE_VEC_INIT(const char *);
    te_string out = TE_STRING_INIT;
    const char *opt;
    const char *p;
    int status;
    te_errno rc;

    fastboot_serial(fb, &args);
    opt = "getvar";
    TE_VEC_APPEND(&args, opt);
    TE_VEC_APPEND(&args, name);

    /* fastboot prints getvar output on stderr as "name: value". */
    rc = tapi_flash_run(factory, "fastboot", "fastboot",
                        (const char **)te_vec_get_mutable(&args, 0),
                        te_vec_size(&args), TAPI_FLASH_QUERY_TIMEOUT_MS,
                        &out, &out, &status);
    te_vec_free(&args);
    if (rc != 0)
        goto out;

    if (status != 0)
    {
        rc = TE_RC(TE_TAPI, TE_EFAIL);
        goto out;
    }

    p = out.ptr != NULL ? strstr(out.ptr, name) : NULL;
    if (p != NULL)
    {
        p = strchr(p, ':');
        if (p != NULL)
        {
            const char *eol;

            p++;
            while (*p == ' ')
                p++;
            eol = strchr(p, '\n');
            te_string_append_buf(value, p,
                                 eol != NULL ? (size_t)(eol - p) : strlen(p));
        }
    }

out:
    te_string_free(&out);

    return rc;
}

/* See description in tapi_flash_fastboot.h */
te_errno
tapi_flash_fastboot_present(tapi_job_factory_t *factory,
                            const tapi_flash_fastboot *fb, bool *present)
{
    const char *args[] = { "devices" };
    te_string out = TE_STRING_INIT;
    te_errno rc;

    *present = false;

    rc = tapi_flash_run(factory, "fastboot", "fastboot", args, 1,
                        TAPI_FLASH_QUERY_TIMEOUT_MS, &out, &out, NULL);
    if (rc == 0 && out.ptr != NULL)
    {
        if (fb->serial != NULL)
            *present = (strstr(out.ptr, fb->serial) != NULL);
        else
            *present = (strstr(out.ptr, "fastboot") != NULL);
    }

    te_string_free(&out);

    return rc;
}
