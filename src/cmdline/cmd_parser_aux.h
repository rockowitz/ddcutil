/* cmd_parser_aux.h
 *
 * Created on: Nov 24, 2015
 *     Author: rock
 *
 * Functions and strings that are independent of the parser
 * package used.
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#ifndef CMD_PARSER_BASE_H_
#define CMD_PARSER_BASE_H_

#include "cmdline/parsed_cmd.h"


typedef
struct {
   int          cmd_id;
   const char * cmd_name;
   int          minchars;
   int          min_arg_ct;
   int          max_arg_ct;
} Cmd_Desc;

Cmd_Desc * find_command(char * cmd);
Cmd_Desc * get_command(int cmdid);
void show_cmd_desc(Cmd_Desc * cmd_desc);   // debugging function

void validate_cmdinfo();

bool all_digits(char * val, int ct);
bool parse_adl_arg(const char * val, int * piAdapterIndex, int * piDisplayIndex);
bool parse_int_arg(char * val, int * pIval);

bool validate_output_level(Parsed_Cmd* parsed_cmd);

extern char * commands_list_help;
extern char * command_argument_help;
extern char * monitor_selection_option_help;
extern char * tracing_comma_separated_option_help;
extern char * tracing_multiple_call_option_help;
extern char * retries_option_help;

#endif /* CMD_PARSER_BASE_H_ */
