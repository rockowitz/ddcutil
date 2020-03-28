/** libyaml_util.h
 */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>

#ifndef LIBYAML_UTIL_H_
#define LIBYAML_UTIL_H_


#include <glib-2.0/glib.h>

typedef enum {
   YAML_PARSE_TOKENS,
   YAML_PARSE_EVENTS,
   YAML_PARSE_DOCUMENT
} Dbg_Yaml_Parse_Mode;

// void dbgrpt_yaml_stream(FILE * fh, int depth);
// void dbgrpt_yaml_tokens(FILE * fh, int depth);
// void dbgrpt_yaml_document(FILE * fh, int depth);

void dbgrpt_yaml_by_file_handle(
      FILE *              fh,
      Dbg_Yaml_Parse_Mode mode,
      int                 depth);
void dbgrpt_yaml_by_filename(
      const char *        filename,
      Dbg_Yaml_Parse_Mode mode,
      int                 depth);
void dbgrpt_yaml_by_string(
      const char *        string,
      Dbg_Yaml_Parse_Mode mode,
      int                 depth);
void dbgrpt_yaml_by_lines(
      char * *         lines,
      Dbg_Yaml_Parse_Mode mode,
      int                 depth);

#endif /* LIBYAML_UTIL_H_ */
