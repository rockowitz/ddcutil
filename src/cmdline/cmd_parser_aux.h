/** \file cmd_parser_aux.c
 *
 *  Functions and strings that are independent of the parser package used.
 */

// Copyright (C) 20014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CMD_PARSER_AUX_H_
#define CMD_PARSER_AUX_H_

#include <string.h>

#include "cmdline/parsed_cmd.h"

void init_cmd_parser_base();

typedef enum {
   Option_None            =  0,
   Option_Explicit_Display = 1
} Cmd_Supported_Options;


typedef
struct {
   int          cmd_id;
   const char * cmd_name;
   int          minchars;
   int          min_arg_ct;
   int          max_arg_ct;
   Cmd_Supported_Options
                supported_options;
} Cmd_Desc;

Cmd_Desc * find_command(char * cmd);
Cmd_Desc * get_command(int cmdid);
void show_cmd_desc(Cmd_Desc * cmd_desc);   // debugging function

bool all_digits(char * val, int ct);
bool parse_dot_separated_arg(  const char * val, int * pv1, int * pv2);
bool parse_colon_separated_arg(const char * val, int * pv1, int * pv2);
bool parse_colon_separated_vid_pid(const char * val, uint16_t * pv1, uint16_t * pv2);
bool parse_int_arg(char * val, int * pIval);

bool parse_feature_id_or_subset(char * val, int cmd_id, Feature_Set_Ref * fsref);
bool parse_feature_ids(char ** vals, int vals_ct, int cmd_id, Feature_Set_Ref * fsref);
Feature_Set_Ref * parse_feature_ids_or_subset(int cmd_id, char **vals, int vals_ct);

bool validate_output_level(Parsed_Cmd* parsed_cmd);

char * assemble_command_argument_help();

extern char * commands_list_help;
extern char * command_argument_help;
extern char * monitor_selection_option_help;
extern char * tracing_comma_separated_option_help;
extern char * tracing_multiple_call_option_help;
extern char * trcfunc_multiple_call_option_help;
extern char * trcfile_multiple_call_option_help;
extern char * stats_multiple_call_option_help;
extern char * maxtries_option_help;

#endif /* CMD_PARSER_AUX_H_ */
