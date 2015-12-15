/* parsed_cmd.h
 *
 * Created on: Nov 24, 2015
 *     Author: rock
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
#define CMDID_INTERROGATE 11
#define CMDID_ENVIRONMENT 12
#define CMDID_END         13    // 1 past last valid CMDID value


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
   int                 max_tries[3];
} Parsed_Cmd;

Parsed_Cmd *  new_parsed_cmd();
void free_parsed_cmd(Parsed_Cmd * parsed_cmd);
void show_parsed_cmd(Parsed_Cmd * parsed_cmd);   // debugging function

#endif /* PARSED_CMD_H_ */
