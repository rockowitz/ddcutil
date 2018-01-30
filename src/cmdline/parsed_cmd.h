/* parsed_cmd.h
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \f
 *
 */

#ifndef PARSED_CMD_H_
#define PARSED_CMD_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>
/** \endcond */

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




typedef enum {
   CMD_FLAG_DDCDATA           = 0x0001,
   CMD_FLAG_FORCE             = 0x0002,
   CMD_FLAG_FORCE_SLAVE_ADDR  = 0x0004,
   CMD_FLAG_TIMESTAMP_TRACE   = 0x0008,  // prepend trace and debug msgs with elapsed time
   CMD_FLAG_SHOW_UNSUPPORTED  = 0x0010,
   CMD_FLAG_ENABLE_FAILSIM    = 0x0020,
   CMD_FLAG_VERIFY            = 0x0040,
   CMD_FLAG_NODETECT          = 0x0080,
   CMD_FLAG_ASYNC             = 0x0100,
   CMD_FLAG_REPORT_FREED_EXCP = 0x2000,
   CMD_FLAG_NOTABLE           = 0x4000
} Parsed_Cmd_Flags;



#define PARSED_CMD_MARKER  "PCMD"
typedef
struct {
   char                marker[4];      // always PCMD
   Cmd_Id_Type         cmd_id;
   int                 argct;
   char *              args[MAX_ARGS];
   Feature_Set_Ref*    fref;
   DDCA_Stats_Type     stats_types;
   char *              failsim_control_fn;
   Display_Identifier* pdid;
   Trace_Group         trace;
   gchar **            traced_files;
   gchar **            traced_functions;
   DDCA_Output_Level   output_level;
   int                 max_tries[3];
   int                 sleep_strategy;
   uint16_t            flags;      // Parsed_Cmd_Flags

   // which?
   DDCA_MCCS_Version_Spec mccs_vspec;
   DDCA_MCCS_Version_Id   mccs_version_id;
} Parsed_Cmd;

Parsed_Cmd *  new_parsed_cmd();
void          free_parsed_cmd(Parsed_Cmd * parsed_cmd);
void          dbgrpt_parsed_cmd(Parsed_Cmd * parsed_cmd, int depth);   // debugging function

#endif /* PARSED_CMD_H_ */
