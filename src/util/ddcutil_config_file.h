/** @file ddcutil_config_file.h  */

// Copyright (C) 2021-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDCUTIL_CONFIG_FILE_H_
#define DDCUTIL_CONFIG_FILE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <glib-2.0/glib.h>

#include "error_info.h"

int tokenize_options_line(
      const char *   string,
      char ***       tokens_loc);

int read_ddcutil_config_file(
      const char *   ddcutil_application,
      char **        config_fn_loc,
      char **        untokenized_option_string_loc,
      GPtrArray *    errmsgs);

int apply_config_file(
      const char * ddcutil_application,     // "ddcutil", "ddcui", "libddcutil"
      int          old_argc,
      char **      old_argv,
      int *        new_argc_loc,
      char ***     new_argv_loc,
      char **      untokenized_option_string_loc,
      char **      configure_fn_loc,
      GPtrArray *  errmsgs);

#ifdef __cplusplus
}    // extern "C"
#endif

#endif /* DDCUTIL_CONFIG_FILE_H_ */
