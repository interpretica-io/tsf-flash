/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Flashing TAPI: internal helpers
 *
 * Internal to tsf-flash; not installed.
 *
 * Every flashing tool here ends up running a program on the agent and
 * reading how it exited and what it printed. tsf-devtool already knows
 * how to do that; this is the argument-vector shape on top of it, plus
 * the small parsing helpers the tool modules share.
 */

#ifndef __TSF_TAPI_FLASH_INTERNAL_H__
#define __TSF_TAPI_FLASH_INTERNAL_H__

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "tapi_job.h"

#include "tapi_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run @p program with @p args on the agent, wait for it and capture
 * what it printed on both streams.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  name         Tool name for log messages.
 * @param[in]  program      Program name or path.
 * @param[in]  args         Arguments after @c argv[0] (may be @c NULL).
 * @param[in]  n_args       Number of @p args.
 * @param[in]  timeout_ms   Timeout, ms.
 * @param[out] out          String to append stdout to (may be @c NULL).
 * @param[out] err          String to append stderr to (may be @c NULL).
 * @param[out] status       Exit status, or @c -1 if the program did not
 *                          exit normally (may be @c NULL).
 *
 * @return Status code of running the program, not of the program.
 */
extern te_errno tapi_flash_run(tapi_job_factory_t *factory, const char *name,
                               const char *program, const char **args,
                               size_t n_args, int timeout_ms, te_string *out,
                               te_string *err, int *status);

/**
 * Run a shell script: @c sh @c -c @p script @c sh @p args. Values reach
 * the script as positional parameters, never pasted into its text.
 *
 * Parameters are those of tapi_flash_run().
 *
 * @return Status code of running the script, not of the script.
 */
extern te_errno tapi_flash_sh(tapi_job_factory_t *factory, const char *name,
                              const char *script, const char **args,
                              size_t n_args, int timeout_ms, te_string *out,
                              te_string *err, int *status);

/**
 * Check whether a program is on the agent, i.e. @c command @c -v.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  program      Program name.
 * @param[out] present      Where to save the answer.
 *
 * @return Status code.
 */
extern te_errno tapi_flash_have_tool(tapi_job_factory_t *factory,
                                     const char *program, bool *present);

/**
 * Take the SHA-256 of a file on the agent, in hex.
 *
 * @param[in]  factory      Job factory.
 * @param[in]  path         Path on the agent.
 * @param[out] digest       String to append the 64-hex digest to.
 * @param[out] present      Set to @c false when the file is not there
 *                          (may be @c NULL).
 *
 * @return Status code.
 */
extern te_errno tapi_flash_sha256(tapi_job_factory_t *factory,
                                  const char *path, te_string *digest,
                                  bool *present);

/**
 * Append a decimal option @c "--name" @c "value" to a vector, keeping
 * the value string alive in @p pool.
 *
 * @param args      Vector of @c const @c char @c * being built.
 * @param pool      Vector of @c char @c * owning the formatted strings.
 * @param name      Option, including any leading dashes.
 * @param value     Value.
 */
extern void tapi_flash_arg_uint(te_vec *args, te_vec *pool, const char *name,
                                uintmax_t value);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSF_TAPI_FLASH_INTERNAL_H__ */
