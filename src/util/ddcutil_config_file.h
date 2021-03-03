// ddcutil_config_file.h

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDCUTIL_CONFIG_FILE_H_
#define DDCUTIL_CONFIG_FILE_H_

int    read_ddcutil_config_file(
      const char *   ddcutil_application,
      char *** tokens_loc,
      char **  default_options_loc);
// char * get_config_file_name();

int merge_command_tokens(
      int      old_argc,
      char **  old_argv,
      int      config_token_ct,
      char **  config_tokens,
      char *** new_argv_loc);

int full_arguments(
      const char * ddcutil_application,     // "ddcutil", "ddcui"
      int      old_argc,
      char **  old_argv,
      char *** new_argv_loc,
      char**   default_options_loc);

#endif /* DDCUTIL_CONFIG_FILE_H_ */
