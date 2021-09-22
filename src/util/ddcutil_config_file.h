// ddcutil_config_file.h

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDCUTIL_CONFIG_FILE_H_
#define DDCUTIL_CONFIG_FILE_H_

#include <glib-2.0/glib.h>

int read_ddcutil_config_file(
      const char *   ddcutil_application,
      char **        config_fn_loc,
      char **        untokenized_option_string_loc,
      GPtrArray *    errmsgs,
      bool           verbose);

int apply_config_file(
      const char * ddcutil_application,     // "ddcutil", "ddcui", "libddcutil"
      int          old_argc,
      char **      old_argv,
      int *        new_argc_loc,
      char ***     new_argv_loc,
      char **      untokenized_option_string_loc,
      char **      configure_fn_loc,
      GPtrArray *  errmsgs);

#endif /* DDCUTIL_CONFIG_FILE_H_ */
