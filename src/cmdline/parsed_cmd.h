/* parsed_cmd.h
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "base/core.h"
#include "base/displays.h"
#include "base/parms.h"


typedef enum {
   CMDID_NONE          =   0x0000,
   CMDID_DETECT        =   0x0001,
   CMDID_CAPABILITIES  =   0x0002,
   CMDID_GETVCP        =   0x0004,
   CMDID_SETVCP        =   0x0008,
   CMDID_LISTVCP       =   0x0010,
   CMDID_TESTCASE      =   0x0020,
   CMDID_LISTTESTS     =   0x0040,
   CMDID_LOADVCP       =   0x0080,
   CMDID_DUMPVCP       =   0x0100,
   CMDID_INTERROGATE   =   0x0200,
   CMDID_ENVIRONMENT   =   0x0400,
   CMDID_USBENV        =   0x0800,
   CMDID_VCPINFO       =   0x1000,
   CMDID_READCHANGES   =   0x2000,
   CMDID_CHKUSBMON     =   0x4000,
   CMDID_PROBE         =   0x8000,
   CMDID_SAVE_SETTINGS = 0x010000,
} Cmd_Id_Type;




#ifdef FUTURE
typedef enum {
   CMD_FLAG_DDCDATA          = 0x01,
   CMD_FLAG_FORCE            = 0x02,
   CMD_FLAG_FORCE_SLAVE_ADDR = 0x04,
   CMD_FLAG_TIMESTAMP_TRACE  = 0x08,
   CMD_FLAG_SHOW_UNSUPPORTED = 0x10,
   CMD_FLAG_ENABLE_FAILSIM   = 0x20,
   CMD_FLAG_VERIFY           = 0x40
} Parsed_Cmd_Flags;
#endif


#define PARSED_CMD_MARKER  "PCMD"
typedef
struct {
   char                marker[4];      // always PCMD
   Cmd_Id_Type         cmd_id;
   int                 argct;
   char *              args[MAX_ARGS];
   Feature_Set_Ref*    fref;
   DDCA_Stats_Type     stats_types;
   bool                ddcdata;
#ifdef OLD
   Msg_Level           msg_level;
   bool                programmatic_output;
#endif
   bool                force;
   bool                force_slave_addr;
   bool                timestamp_trace;    // prepend trace and debug msgs with elapsed time
   bool                show_unsupported;
   bool                enable_failure_simulation;
   bool                verify_setvcp;
// bool                nodetect;
   char *              failsim_control_fn;
   Display_Identifier* pdid;
   Trace_Group         trace;
   DDCA_Output_Level   output_level;
   int                 max_tries[3];
   int                 sleep_strategy;
#ifdef FUTURE
   uint16_t            flags;      // Parsed_Cmd_Flags
#endif
} Parsed_Cmd;

Parsed_Cmd *  new_parsed_cmd();
void          free_parsed_cmd(Parsed_Cmd * parsed_cmd);
void          report_parsed_cmd(Parsed_Cmd * parsed_cmd, int depth);   // debugging function

#endif /* PARSED_CMD_H_ */
