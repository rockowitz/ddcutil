/** @file parsed_cmd.h
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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
   CMD_FLAG_DDCDATA             = 0x0001,
   CMD_FLAG_FORCE               = 0x0002,
   CMD_FLAG_FORCE_SLAVE_ADDR    = 0x0004,
   CMD_FLAG_TIMESTAMP_TRACE     = 0x0008,  // prepend trace and debug msgs with elapsed time
   CMD_FLAG_SHOW_UNSUPPORTED    = 0x0010,
   CMD_FLAG_ENABLE_FAILSIM      = 0x0020,
   CMD_FLAG_VERIFY              = 0x0040,
   CMD_FLAG_NODETECT            = 0x0080,
   CMD_FLAG_ASYNC               = 0x0100,
   CMD_FLAG_REPORT_FREED_EXCP   = 0x0200,
   CMD_FLAG_NOTABLE             = 0x0400,
   CMD_FLAG_RW_ONLY           = 0x010000,
   CMD_FLAG_RO_ONLY           = 0x020000,
   CMD_FLAG_WO_ONLY           = 0x040000,
   CMD_FLAG_ENABLE_UDF        = 0x100000,
   CMD_FLAG_ENABLE_USB             = 0x200000,
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
   DDCA_Trace_Group         traced_groups;
   gchar **            traced_files;
   gchar **            traced_functions;
   DDCA_Output_Level   output_level;
   int                 max_tries[3];
   float               sleep_multiplier;
   uint32_t            flags;      // Parsed_Cmd_Flags
   int                 i1;         // available for temporary use

   // which?
   DDCA_MCCS_Version_Spec mccs_vspec;
   DDCA_MCCS_Version_Id   mccs_version_id;
} Parsed_Cmd;

Parsed_Cmd *  new_parsed_cmd();
void          free_parsed_cmd(Parsed_Cmd * parsed_cmd);
void          dbgrpt_parsed_cmd(Parsed_Cmd * parsed_cmd, int depth);   // debugging function

#endif /* PARSED_CMD_H_ */
