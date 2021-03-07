// ddcutil_config_file.h

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDCUTIL_CONFIG_FILE_H_
#define DDCUTIL_CONFIG_FILE_H_

#include <glib-2.0/glib.h>

int read_ddcutil_config_file(
      const char *   ddcutil_application,
      char ***       tokenized_options_loc,
      char**         untokenized_option_string_loc,
      GPtrArray *    errmsgs,
      char **        config_fn_loc,
      bool           verbose);

int merge_command_tokens(
      int      old_argc,
      char **  old_argv,
      int      config_token_ct,
      char **  config_tokens,
      char *** new_argv_loc);

int read_and_parse_config_file(
      const char * ddcutil_application,     // "ddcutil", "ddcui"
      int          old_argc,
      char **      old_argv,
      char ***     new_argv_loc,
      char**       untokenized_cmd_prefix_loc,
      char**       configure_fn_loc,
      GPtrArray *  errmsgs);

#endif /* DDCUTIL_CONFIG_FILE_H_ */
