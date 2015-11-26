/*
 * parsed_cmd.h
 *
 *  Created on: Nov 24, 2015
 *      Author: rock
 */

#ifndef PARSED_CMD_H_
#define PARSED_CMD_H_

#include <stdbool.h>

#include "base/parms.h"


#define CMDID_NONE         0
#define CMDID_DETECT       1
#define CMDID_INFO         2
#define CMDID_CAPABILITIES 3
#define CMDID_GETVCP       4
#define CMDID_SETVCP       5
#define CMDID_LISTVCP      6
#define CMDID_TESTCASE     7
#define CMDID_LISTTESTS    8
#define CMDID_LOADVCP      9
#define CMDID_DUMPVCP     10
#define CMDID_END         11    // 1 past last valid CMDID value


#define PARSED_CMD_MARKER  "PCMD"
typedef
struct {
   char                marker[4];      // always PCMD
   int                 cmd_id;
   int                 argct;
   char *              args[MAX_ARGS];
   bool                stats;
   bool                ddcdata;
#ifdef OLD
   Msg_Level           msg_level;
   bool                programmatic_output;
#endif
   bool                force;
   Display_Identifier* pdid;
   Trace_Group         trace;
   Output_Level        output_level;   // new, to replace msg_level and programmatic_output
} Parsed_Cmd;

Parsed_Cmd *  new_parsed_cmd();
void free_parsed_cmd(Parsed_Cmd * parsed_cmd);
void show_parsed_cmd(Parsed_Cmd * parsedCmd);   // debugging function

#endif /* PARSED_CMD_H_ */
