/** @file cmd_parser.h
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

//
// This simple interface facilitated switching command parsers during
// development.  Candidate for removal.
//

#ifndef CMD_PARSER_H_
#define CMD_PARSER_H_

#include <glib-2.0/glib.h>

#include "public/ddcutil_types.h"

#include "cmdline/parsed_cmd.h"

bool parse_syslog_level(
      const char *        sval,
      DDCA_Syslog_Level * result_loc,
      GPtrArray *         errmsgs);

Parsed_Cmd * parse_command(
      int         argc,
      char *      argv[],
      Parser_Mode parser_mode,
      GPtrArray * errmsgs);

#endif /* CMD_PARSER_H_ */
