/* cmd_parser_popt.h
 *
 *  Created on: Jun 16, 2014
 *      Author: rock
 */

#ifndef CMD_PARSER_POPT_H_
#define CMD_PARSER_POPT_H_

#include "base/common.h"
#include "base/displays.h"
#include "base/msg_control.h"
#include "base/util.h"

#include "cmdline/parsed_cmd.h"


Parsed_Cmd * parse_command(int argc, char * argv[]);
// Parsed_Cmd * parse_command_goption(int argc, char * argv[]);

#endif /* CMD_PARSER_POPT_H_ */
