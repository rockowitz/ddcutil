/** @file parsed_cmd.h
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
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
   MODE_DDCUTIL,
   MODE_LIBDDCUTIL
} Parser_Mode;

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
#ifdef ENABLE_ENVCMDS
   CMDID_INTERROGATE   =   0x0200,
   CMDID_ENVIRONMENT   =   0x0400,
   CMDID_USBENV        =   0x0800,
#endif
   CMDID_VCPINFO       =   0x1000,
   CMDID_READCHANGES   =   0x2000,
   CMDID_CHKUSBMON     =   0x4000,
   CMDID_PROBE         =   0x8000,
   CMDID_SAVE_SETTINGS = 0x010000,
   CMDID_DISCARD_CACHE = 0x020000,
   CMDID_C1            = 0x100000,         // utility command id, for tests
} Cmd_Id_Type;

typedef enum {
   CMD_FLAG_DDCDATA                  = 0x0001,
   CMD_FLAG_FORCE                    = 0x0002,
   CMD_FLAG_FORCE_SLAVE_ADDR         = 0x0004,
   CMD_FLAG_TIMESTAMP_TRACE          = 0x0008,  // prepend trace and debug msgs with elapsed time
   CMD_FLAG_SHOW_UNSUPPORTED         = 0x0010,
   CMD_FLAG_ENABLE_FAILSIM           = 0x0020,
   CMD_FLAG_VERIFY                   = 0x0040,
#ifdef OLD
   CMD_FLAG_NODETECT                 = 0x0080,  // UNUSED
#endif
   CMD_FLAG_ASYNC                    = 0x0100,
   CMD_FLAG_REPORT_FREED_EXCP        = 0x0200,
   CMD_FLAG_NOTABLE                  = 0x0400,
   CMD_FLAG_THREAD_ID_TRACE          = 0x0800,
   CMD_FLAG_RW_ONLY                = 0x010000,
   CMD_FLAG_RO_ONLY                = 0x020000,
   CMD_FLAG_WO_ONLY                = 0x040000,
   CMD_FLAG_ENABLE_UDF             = 0x100000,
   CMD_FLAG_ENABLE_USB             = 0x200000,
#ifdef OLD
   CMD_FLAG_TIMEOUT_I2C_IO         = 0x400000,  // UNUSED  --timeout-i2c-io
   CMD_FLAG_REDUCE_SLEEPS          = 0x800000,  // --sleep-less, etc
#endif
   CMD_FLAG_F1                   = 0x01000000,
   CMD_FLAG_F2                   = 0x02000000,
   CMD_FLAG_F3                   = 0x04000000,
   CMD_FLAG_F4                   = 0x08000000,
   CMD_FLAG_F5                   = 0x10000000,
   CMD_FLAG_F6                   = 0x20000000,
   CMD_FLAG_DEFER_SLEEPS         = 0x80000000,
   CMD_FLAG_X52_NO_FIFO        = 0x0100000000,
   CMD_FLAG_VERBOSE_STATS      = 0x0200000000,
   CMD_FLAG_SHOW_SETTINGS      = 0x0400000000,
   CMD_FLAG_ENABLE_CACHED_CAPABILITIES
                               = 0x0800000000,
// CMD_FLAG_CLEAR_PERSISTENT_CACHE = 0x1000000000,
   CMD_FLAG_WALLTIME_TRACE     = 0x2000000000,
   CMD_FLAG_I2C_IO_FILEIO    = 0x010000000000,
   CMD_FLAG_I2C_IO_IOCTL     = 0x020000000000,
   CMD_FLAG_I1_SET           = 0x040000000000,
   CMD_FLAG_I2_SET           = 0x080000000000,
   CMD_FLAG_EXPLICIT_SLEEP_MULTIPLIER
                             = 0x100000000000,
   CMD_FLAG_DSA2             = 0x200000000000,
   CMD_FLAG_QUICK            = 0x800000000000,
   CMD_FLAG_F7             = 0x01000000000000,
   CMD_FLAG_F8             = 0x02000000000000,
   CMD_FLAG_MOCK           = 0x04000000000000,
   CMD_FLAG_PROFILE_API    = 0x08000000000000,
   CMD_FLAG_FL1_SET        = 0x10000000000000,
   CMD_FLAG_FL2_SET        = 0x20000000000000,
   CMD_FLAG_ENABLE_CACHED_DISPLAYS
                           = 0x40000000000000,
   CMD_FLAG_TRACE_TO_SYSLOG_ONLY
                           = 0x80000000000000,
   CMD_FLAG_STATS_TO_SYSLOG
                         = 0x0100000000000000,
   CMD_FLAG_INTERNAL_STATS
                         = 0x0200000000000000,
   CMD_FLAG_EXPLICIT_I2C_SOURCE_ADDR
                        =  0x0400000000000000,
} Parsed_Cmd_Flags;

#define IGNORED_VID_PID_MAX 4

typedef
enum {VALUE_TYPE_ABSOLUTE,
      VALUE_TYPE_RELATIVE_PLUS,
      VALUE_TYPE_RELATIVE_MINUS
} Setvcp_Value_Type;

typedef
struct {
   Byte              feature_code;
   Setvcp_Value_Type feature_value_type;
   char *            feature_value;
} Parsed_Setvcp_Args;

typedef
enum {NO_CACHES  = 0,
      CAPABILITIES_CACHE = 1,
      DISPLAYS_CACHE = 2,
      DSA2_CACHE     = 4,
      ALL_CACHES     = 255
} Cache_Types;

/** Parsed arguments to **ddcutil** command  */
#define PARSED_CMD_MARKER  "PCMD"
typedef
struct {
   char                   marker[4];      // always PCMD
   char *                 raw_command;
   Parser_Mode            parser_mode;
   int                    argct;
   char *                 args[MAX_ARGS];
   Cmd_Id_Type            cmd_id;
   Feature_Set_Ref*       fref;
   GArray *               setvcp_values;
   DDCA_Stats_Type        stats_types;
   char *                 failsim_control_fn;
   Display_Identifier*    pdid;
// Display_Selector*      display_selector;   // for future use

   DDCA_Trace_Group       traced_groups;
   gchar **               traced_files;
   gchar **               traced_functions;
   gchar **               traced_calls;
   gchar **               traced_api_calls;
   DDCA_Syslog_Level      syslog_level;
   char *                 trace_destination;

   DDCA_Output_Level      output_level;
   Cache_Types            cache_types;
   uint16_t               max_tries[3];
   float                  sleep_multiplier;
   DDCA_MCCS_Version_Spec mccs_vspec;
// DDCA_MCCS_Version_Id   mccs_version_id;
   int                    edid_read_size;
   uint64_t               flags;      // Parsed_Cmd_Flags
   Bit_Set_32             ignored_hiddevs;
   uint8_t                ignored_usb_vid_pid_ct;
   uint8_t                explicit_i2c_source_addr;
   uint32_t               ignored_usb_vid_pids[IGNORED_VID_PID_MAX];

   int                    i1;         // for temporary use
   int                    i2;         // for temporary use
   char *                 s1;         // for temporary use
   char *                 s2;         // for temporary use
   char *                 s3;         // for temporary use
   char *                 s4;         // for temporary use
   float                  fl1;        // for temporary use
   float                  fl2;        // for temporary use
} Parsed_Cmd;

#ifdef UNUSED
/** Preparsed arguments to **ddcutil** command */
typedef struct {
   DDCA_Syslog_Level severity;
   bool                 verbose;
   bool                 noconfig;
} Preparsed_Cmd;
#endif

const char *  parser_mode_name(Parser_Mode mode);
const char *  cmdid_name(Cmd_Id_Type id);
const char *  setvcp_value_type_name(Setvcp_Value_Type value_type);
Parsed_Cmd *  new_parsed_cmd();
void          free_parsed_cmd(Parsed_Cmd * parsed_cmd);
void          dbgrpt_parsed_cmd(Parsed_Cmd * parsed_cmd, int depth);

#endif /* PARSED_CMD_H_ */
