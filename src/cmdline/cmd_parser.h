/** @file cmd_parser.h
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

//
// This simple interface facilitated switching command parsers during
// development.  Candidate for removal.
//

#ifndef CMD_PARSER_H_
#define CMD_PARSER_H_

#include "cmdline/parsed_cmd.h"

Parsed_Cmd * parse_command(int argc, char * argv[], Parser_Mode mode);

#endif /* CMD_PARSER_H_ */
