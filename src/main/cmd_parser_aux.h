/* cmd_parser_aux.h
 *
 *  Created on: Nov 24, 2015
 *      Author: rock
 */

#ifndef CMD_PARSER_BASE_H_
#define CMD_PARSER_BASE_H_


#include "main/parsed_cmd.h"


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
void show_cmd_desc(Cmd_Desc * cmd_desc);

void validate_cmdinfo();

bool all_digits(char * val, int ct);
bool parse_adl_arg(const char * val, int * piAdapterIndex, int * piDisplayIndex);
bool parse_int_arg(char * val, int * pIval);

bool validate_output_level(Parsed_Cmd* parsed_cmd);

#endif /* CMD_PARSER_BASE_H_ */
