/** @file subprocess_util.h
 *
 * Functions to execute shell commands
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SUBPROCESS_UTIL_H_
#define SUBPROCESS_UTIL_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>
/** \endcond */

bool        execute_shell_cmd(                const char * shell_cmd);
bool        execute_shell_cmd_rpt(            const char * shell_cmd, int depth);
GPtrArray * execute_shell_cmd_collect(        const char * shell_cmd);
int         execute_cmd_collect_with_filter(
               const char * shell_cmd,
               char **      filter_terms,
               bool         ignore_case,
               int          limit,
               GPtrArray ** result_loc);

char *      execute_shell_cmd_one_line_result(const char * shell_cmd);
bool        is_command_in_path(               const char * cmd);
int         test_command_executability(       const char * cmd);

#endif /* SUBPROCESS_UTIL_H_ */
