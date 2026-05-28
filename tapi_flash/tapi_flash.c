/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing a device under test
 *
 * The image, the result, tool detection, verification and waiting for a
 * device across a reset.
 */

#define TE_LGR_USER "TAPI FLASH"

#include "te_config.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "te_sleep.h"
#include "te_str.h"
#include "te_string.h"
#include "te_time.h"
#include "tapi_job_opt.h"

#include "tapi_devtool_run.h"

#include "tapi_flash.h"
#include "tapi_flash_internal.h"

/** Arguments of a command, as a plain vector of strings. */
typedef struct flash_cmd_opt {
    size_t n_args;
    const char **args;
} flash_cmd_opt;

static const tapi_job_opt_bind flash_cmd_binds[] = TAPI_JOB_OPT_SET(
    TAPI_JOB_OPT_ARRAY_PTR(flash_cmd_opt, n_args, args,
        TAPI_JOB_OPT_CONTENT(TAPI_JOB_OPT_STRING, NULL, false))
);

/* See description in tapi_flash_internal.h */
te_errno
tapi_flash_run(tapi_job_factory_t *factory, const char *name,
               const char *program, const char **args, size_t n_args,
               int timeout_ms, te_string *out, te_string *err, int *status)
{
    flash_cmd_opt opt = { .n_args = n_args, .args = args };
    tapi_devtool_run run = TAPI_DEVTOOL_RUN_INIT;
    tapi_devtool_output output;
    te_errno rc;

    rc = tapi_devtool_run_init(&run, factory, name, program, flash_cmd_binds,
                               &opt, NULL);
    if (rc != 0)
        return rc;

    rc = tapi_devtool_run_start(&run);
    if (rc == 0)
        rc = tapi_devtool_run_wait(&run, timeout_ms);

    if (rc == 0)
    {
        tapi_devtool_run_get_output(&run, &output);
        if (out != NULL)
            te_string_append(out, "%s", output.out);
        if (err != NULL)
            te_string_append(err, "%s", output.err);
        if (status != NULL)
            *status = (output.status.type == TAPI_JOB_STATUS_EXITED) ?
                      output.status.value : -1;
    }

    tapi_devtool_run_fini(&run);

    return rc;
}

/* See description in tapi_flash_internal.h */
te_errno
tapi_flash_sh(tapi_job_factory_t *factory, const char *name,
              const char *script, const char **args, size_t n_args,
              int timeout_ms, te_string *out, te_string *err, int *status)
{
    const char **argv;
    size_t i;
    te_errno rc;

    argv = TE_ALLOC((n_args + 3) * sizeof(*argv));
    argv[0] = "-c";
    argv[1] = script;
    argv[2] = "sh";
    for (i = 0; i < n_args; i++)
        argv[i + 3] = args[i];

    rc = tapi_flash_run(factory, name, "sh", argv, n_args + 3, timeout_ms,
                        out, err, status);

    free(argv);

    return rc;
}

/* See description in tapi_flash_internal.h */
te_errno
tapi_flash_have_tool(tapi_job_factory_t *factory, const char *program,
                     bool *present)
{
    const char *args[] = { program };
    int status;
    te_errno rc;

    rc = tapi_flash_sh(factory, "which", "command -v \"$1\" >/dev/null", args,
                       1, TAPI_FLASH_QUERY_TIMEOUT_MS, NULL, NULL, &status);
    if (rc == 0)
        *present = (status == 0);

    return rc;
}

/* See description in tapi_flash_internal.h */
te_errno
tapi_flash_sha256(tapi_job_factory_t *factory, const char *path,
                  te_string *digest, bool *present)
{
    const char *args[] = { path };
    te_string out = TE_STRING_INIT;
    int status;
    te_errno rc;

    if (present != NULL)
        *present = false;

    rc = tapi_flash_run(factory, "sha256sum", "sha256sum", args, 1,
                        TAPI_FLASH_QUERY_TIMEOUT_MS, &out, NULL, &status);
    if (rc != 0)
        goto out;

    if (status != 0 || out.len < 64)
    {
        if (present == NULL)
        {
            ERROR("Cannot hash %s on the agent", path);
            rc = TE_RC(TE_TAPI, TE_ENOENT);
        }
        goto out;
    }

    if (present != NULL)
        *present = true;
    te_string_append_buf(digest, out.ptr, 64);

out:
    te_string_free(&out);

    return rc;
}

/* See description in tapi_flash_internal.h */
void
tapi_flash_arg_uint(te_vec *args, te_vec *pool, const char *name,
                    uintmax_t value)
{
    char *s = te_string_fmt("%ju", value);
    const char *cname = name;
    const char *cs = s;

    TE_VEC_APPEND(pool, s);
    if (name != NULL)
        TE_VEC_APPEND(args, cname);
    TE_VEC_APPEND(args, cs);
}

/* See description in tapi_flash.h */
te_errno
tapi_flash_image_init(tapi_job_factory_t *factory, const char *path,
                      const char *format, tapi_flash_image *image)
{
    const char *args[] = { "-c", "%s", path };
    te_string size = TE_STRING_INIT;
    te_string digest = TE_STRING_INIT;
    int status;
    te_errno rc;

    memset(image, 0, sizeof(*image));
    image->path = TE_STRDUP(path);
    if (format != NULL)
        image->format = TE_STRDUP(format);

    rc = tapi_flash_run(factory, "stat", "stat", args, TE_ARRAY_LEN(args),
                        TAPI_FLASH_QUERY_TIMEOUT_MS, &size, NULL, &status);
    if (rc != 0)
        goto out;
    if (status != 0)
    {
        ERROR("The image %s is not on the agent", path);
        rc = TE_RC(TE_TAPI, TE_ENOENT);
        goto out;
    }
    image->size = strtoull(size.ptr != NULL ? size.ptr : "0", NULL, 10);

    rc = tapi_flash_sha256(factory, path, &digest, NULL);
    if (rc == 0)
        te_strlcpy(image->digest, digest.ptr, sizeof(image->digest));

out:
    te_string_free(&size);
    te_string_free(&digest);
    if (rc != 0)
        tapi_flash_image_free(image);

    return rc;
}

/* See description in tapi_flash.h */
void
tapi_flash_image_free(tapi_flash_image *image)
{
    free(image->path);
    free(image->format);
    image->path = NULL;
    image->format = NULL;
}

/* See description in tapi_flash.h */
void
tapi_flash_result_init(tapi_flash_result *result)
{
    memset(result, 0, sizeof(*result));
    result->status = -1;
    result->output = (te_string)TE_STRING_INIT;
}

/* See description in tapi_flash.h */
void
tapi_flash_result_log(const tapi_flash_result *result, const char *what)
{
    RING("%s: %s, exit %d, %" PRIu64 " bytes in %.1f s\n%s", what,
         result->ok ? "ok" : "FAILED", result->status, result->bytes,
         result->seconds,
         result->output.ptr != NULL ? result->output.ptr : "");
}

/* See description in tapi_flash.h */
void
tapi_flash_result_free(tapi_flash_result *result)
{
    te_string_free(&result->output);
}

/* See description in tapi_flash.h */
te_errno
tapi_flash_tools_probe(tapi_job_factory_t *factory, tapi_flash_tools *tools)
{
    static const struct {
        const char *program;
        size_t offset;
    } probe[] = {
        { "dfu-util", offsetof(tapi_flash_tools, dfu_util) },
        { "fastboot", offsetof(tapi_flash_tools, fastboot) },
        { "flashrom", offsetof(tapi_flash_tools, flashrom) },
        { "openocd", offsetof(tapi_flash_tools, openocd) },
        { "flashcp", offsetof(tapi_flash_tools, flashcp) },
        { "nandwrite", offsetof(tapi_flash_tools, nandwrite) },
        { "flash_erase", offsetof(tapi_flash_tools, flash_erase) },
        { "dd", offsetof(tapi_flash_tools, dd) },
    };
    bool esptool = false;
    bool esptool_py = false;
    size_t i;
    te_errno rc;

    memset(tools, 0, sizeof(*tools));

    for (i = 0; i < TE_ARRAY_LEN(probe); i++)
    {
        bool present = false;

        rc = tapi_flash_have_tool(factory, probe[i].program, &present);
        if (rc != 0)
            return rc;
        *(bool *)((char *)tools + probe[i].offset) = present;
    }

    rc = tapi_flash_have_tool(factory, "esptool", &esptool);
    if (rc == 0)
        rc = tapi_flash_have_tool(factory, "esptool.py", &esptool_py);
    tools->esptool = esptool || esptool_py;

    return rc;
}

/* See description in tapi_flash.h */
te_errno
tapi_flash_verify(tapi_job_factory_t *factory, const tapi_flash_image *image,
                  const char *readback,
                  te_errno (*read_region)(void *ctx, const char *dest,
                                          uint64_t size),
                  void *ctx, tapi_kernel_issues *issues, bool *matched)
{
    te_string cmp = TE_STRING_INIT;
    te_string trimmed = TE_STRING_INIT;
    char *fmt = NULL;
    int status;
    bool ok = false;
    te_errno rc;

    if (matched != NULL)
        *matched = false;

    rc = read_region(ctx, readback, image->size);
    if (rc != 0)
    {
        ERROR("Cannot read the device back for verification: %r", rc);
        return rc;
    }

    /*
     * Hash exactly as many bytes as were written: a read back padded to
     * an erase block is longer, and its tail is not part of the image.
     */
    fmt = te_string_fmt("head -c %" PRIu64 " \"$1\" | sha256sum", image->size);
    rc = tapi_flash_sh(factory, "verify", fmt, &readback, 1,
                       TAPI_FLASH_TIMEOUT_MS, &cmp, NULL, &status);
    if (rc != 0)
        goto out;

    if (status == 0 && cmp.len >= 64)
    {
        te_string_append_buf(&trimmed, cmp.ptr, 64);
        ok = (strcasecmp(trimmed.ptr, image->digest) == 0);
    }

    if (matched != NULL)
        *matched = ok;

    if (ok)
    {
        RING("Verify of %s against the device: match (%s)", image->path,
             image->digest);
    }
    else if (issues != NULL)
    {
        tapi_kernel_issues_add(issues, "flash.verify", image->path,
                               "the device does not hold what was written: "
                               "image %s, device %s", image->digest,
                               trimmed.len > 0 ? trimmed.ptr : "unreadable");
    }

out:
    free(fmt);
    te_string_free(&cmp);
    te_string_free(&trimmed);

    return rc;
}

/* See description in tapi_flash.h */
te_errno
tapi_flash_wait_reappear(tapi_job_factory_t *factory, const char *subject,
                         te_errno (*present)(void *ctx, bool *visible),
                         void *ctx, int timeout_ms, int poll_ms,
                         tapi_kernel_issues *issues, bool *returned)
{
    struct timeval start;
    bool went_away = false;
    bool visible;
    te_errno rc;

    (void)factory;

    if (returned != NULL)
        *returned = false;

    if (poll_ms <= 0)
        poll_ms = 200;

    rc = present(ctx, &visible);
    if (rc != 0)
        return rc;

    gettimeofday(&start, NULL);
    for (;;)
    {
        struct timeval now;
        long elapsed;

        rc = present(ctx, &visible);
        if (rc != 0)
            return rc;

        if (!visible)
            went_away = true;
        else if (went_away)
        {
            if (returned != NULL)
                *returned = true;
            RING("%s went away and came back", subject);
            return 0;
        }

        gettimeofday(&now, NULL);
        elapsed = (now.tv_sec - start.tv_sec) * 1000 +
                  (now.tv_usec - start.tv_usec) / 1000;
        if (elapsed >= timeout_ms)
            break;

        te_msleep(poll_ms);
    }

    if (!went_away)
    {
        /* Reset too quick to catch is not a failure. */
        RING("%s stayed visible throughout; assuming the reset was quick",
             subject);
        if (returned != NULL)
            *returned = true;
        return 0;
    }

    if (issues != NULL)
        tapi_kernel_issues_add(issues, "flash.no-return", subject,
                               "the device went away after flashing and did "
                               "not come back within %d ms", timeout_ms);

    return 0;
}
