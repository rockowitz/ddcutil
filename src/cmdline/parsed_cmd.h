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
   CMDID_LIST_RTTI     = 0x040000,
   CMDID_NOOP          = 0x080000,
   CMDID_C1            = 0x100000,         // utility command id, for tests
   CMDID_C2            = 0x200000,
   CMDID_C3            = 0x400000,
   CMDID_C4            = 0x800000,
} Cmd_Id_Type;

typedef enum {
   CMD_FLAG_DDCDATA                  = 0x0001,
   CMD_FLAG_FORCE_UNRECOGNIZED_VCP_CODE
                                     = 0x0002,
   CMD_FLAG_FORCE_SLAVE_ADDR         = 0x0004,
   CMD_FLAG_TIMESTAMP_TRACE          = 0x0008,  // prepend trace and debug msgs with elapsed time
   CMD_FLAG_SHOW_UNSUPPORTED         = 0x0010,
   CMD_FLAG_ENABLE_FAILSIM           = 0x0020,
   CMD_FLAG_VERIFY                   = 0x0040,
   CMD_FLAG_SKIP_DDC_CHECKS          = 0x0080,

   CMD_FLAG_UNUSED1                  = 0x0100,
   CMD_FLAG_REPORT_FREED_EXCP        = 0x0200,
   CMD_FLAG_NOTABLE                  = 0x0400,
   CMD_FLAG_THREAD_ID_TRACE          = 0x0800,
   CMD_FLAG_NULL_MSG_INDICATES_UNSUPPORTED_FEATURE
                                     = 0x1000,
   CMD_FLAG_HEURISTIC_UNSUPPORTED_FEATURES
                                     = 0x2000,
   CMD_FLAG_DISCARD_CACHES           = 0x4000,
   CMD_FLAG_PROCESS_ID_TRACE         = 0x8000,

   CMD_FLAG_RW_ONLY                = 0x010000,
   CMD_FLAG_RO_ONLY                = 0x020000,
   CMD_FLAG_WO_ONLY                = 0x040000,
   CMD_FLAG_ASYNC_I2C_CHECK        = 0x080000,

   CMD_FLAG_ENABLE_UDF             = 0x100000,
   CMD_FLAG_ENABLE_USB             = 0x200000,

   CMD_FLAG_TRY_GET_EDID_FROM_SYSFS
                                 = 0x10000000,
   CMD_FLAG_FLOCK                = 0x20000000,
   CMD_FLAG_DEFER_SLEEPS         = 0x40000000,

   CMD_FLAG_X52_NO_FIFO        = 0x0100000000,
   CMD_FLAG_VERBOSE_STATS      = 0x0200000000,
   CMD_FLAG_SHOW_SETTINGS      = 0x0400000000,
   CMD_FLAG_ENABLE_CACHED_CAPABILITIES
                               = 0x0800000000,

// CMD_FLAG_CLEAR_PERSISTENT_CACHE
//                             = 0x1000000000,
   CMD_FLAG_WALLTIME_TRACE     = 0x2000000000,

   CMD_FLAG_I2C_IO_FILEIO    = 0x010000000000,
   CMD_FLAG_I2C_IO_IOCTL     = 0x020000000000,

   CMD_FLAG_EXPLICIT_SLEEP_MULTIPLIER
                             = 0x100000000000,
   CMD_FLAG_DSA2             = 0x200000000000,

   CMD_FLAG_QUICK            = 0x400000000000,


   CMD_FLAG_MOCK           = 0x01000000000000,
   CMD_FLAG_PROFILE_API    = 0x02000000000000,

   CMD_FLAG_ENABLE_CACHED_DISPLAYS
                           = 0x10000000000000,
   CMD_FLAG_TRACE_TO_SYSLOG_ONLY
                           = 0x20000000000000,
   CMD_FLAG_TRACE_TO_SYSLOG= 0x40000000000000,
   CMD_FLAG_STATS_TO_SYSLOG
                         = 0x0100000000000000,
   CMD_FLAG_INTERNAL_STATS
                         = 0x0200000000000000,
   CMD_FLAG_EXPLICIT_I2C_SOURCE_ADDR
                        =  0x0400000000000000,

   CMD_FLAG_ENABLE_TRACED_FUNCTION_STACK
                         = 0x1000000000000000,
   CMD_FLAG_TRACED_FUNCTION_STACK_ERRORS_FATAL
                         = 0x2000000000000000,
   CMD_FLAG_DISABLE_API =  0x4000000000000000,
   CMD_FLAG_WATCH_DISPLAY_EVENTS
                        =  0x8000000000000000,


#ifdef OLD
   CMD_FLAG_TIMEOUT_I2C_IO         = 0x400000,  // UNUSED  --timeout-i2c-io
   CMD_FLAG_REDUCE_SLEEPS          = 0x800000,  // --sleep-less, etc
   CMD_FLAG_NODETECT                 = 0x0080,  // UNUSED
#endif

} Parsed_Cmd_Flags;

typedef enum {
   CMD_FLAG2_F1                   = 0x00000001,
   CMD_FLAG2_F2                   = 0x00000002,
   CMD_FLAG2_F3                   = 0x00000004,
   CMD_FLAG2_F4                   = 0x00000008,
   CMD_FLAG2_F5                   = 0x00000010,
   CMD_FLAG2_F6                   = 0x00000020,
   CMD_FLAG2_F7                   = 0x00000040,
   CMD_FLAG2_F8                   = 0x00000080,
   CMD_FLAG2_F9                   = 0x00000100,
   CMD_FLAG2_F10                  = 0x00000200,
   CMD_FLAG2_F11                  = 0x00000400,
   CMD_FLAG2_F12                  = 0x00000800,
   CMD_FLAG2_F13                  = 0x00001000,
   CMD_FLAG2_F14                  = 0x00002000,
   CMD_FLAG2_F15                  = 0x00004000,
   CMD_FLAG2_F16                  = 0x00008000,
   CMD_FLAG2_F17                  = 0x00010000,
   CMD_FLAG2_F18                  = 0x00020000,
   CMD_FLAG2_F19                  = 0x00040000,
   CMD_FLAG2_F20                  = 0x00080000,
   CMD_FLAG2_F21                  = 0x00100000,
   CMD_FLAG2_F22                  = 0x00200000,
   CMD_FLAG2_F23                  = 0x00400000,
   CMD_FLAG2_F24                  = 0x00800000,
   CMD_FLAG2_F25                  = 0x01000000,
   CMD_FLAG2_F26                  = 0x02000000,
   CMD_FLAG2_F27                  = 0x04000000,
   CMD_FLAG2_F28                  = 0x08000000,
   CMD_FLAG2_F29                  = 0x10000000,
   CMD_FLAG2_F30                  = 0x20000000,
   CMD_FLAG2_F31                  = 0x40000000,
   CMD_FLAG2_F32                  = 0x80000000,

   CMD_FLAG2_I1_SET           = 0x010000000000,
   CMD_FLAG2_I2_SET           = 0x020000000000,
   CMD_FLAG2_I3_SET           = 0x040000000000,
   CMD_FLAG2_I4_SET           = 0x080000000000,
   CMD_FLAG2_I5_SET           = 0x100000000000,
   CMD_FLAG2_I6_SET           = 0x200000000000,
   CMD_FLAG2_I7_SET           = 0x400000000000,
   CMD_FLAG2_I8_SET           = 0x800000000000,
   CMD_FLAG2_I9_SET         = 0x01000000000000,
   CMD_FLAG2_I10_SET        = 0x02000000000000,
   CMD_FLAG2_I11_SET        = 0x04000000000000,
   CMD_FLAG2_I12_SET        = 0x08000000000000,
   CMD_FLAG2_I13_SET        = 0x10000000000000,
   CMD_FLAG2_I14_SET        = 0x20000000000000,
   CMD_FLAG2_I15_SET        = 0x40000000000000,
   CMD_FLAG2_I16_SET        = 0x80000000000000,
   CMD_FLAG2_FL1_SET      = 0x1000000000000000,
   CMD_FLAG2_FL2_SET      = 0x2000000000000000,

} Parsed_Cmd_Flags2;

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
enum {NO_CACHES          = 0,
      CAPABILITIES_CACHE = 1,
      DISPLAYS_CACHE     = 2,
      DSA2_CACHE         = 4,
      ALL_CACHES         = 255
} Cache_Types;

/** Parsed arguments to **ddcutil** command  */
#define PARSED_CMD_MARKER  "PCMD"
typedef
struct {
   char                   marker[4];      // always PCMD

   // General
   char *                 raw_command;
   Parser_Mode            parser_mode;
   int                    argct;
   char *                 args[MAX_ARGS];
   uint64_t               flags;      // Parsed_Cmd_Flags
   uint64_t               flags2;     // Parsed_Cmd_Flags2
   DDCA_Output_Level      output_level;
   DDCA_MCCS_Version_Spec mccs_vspec;
// DDCA_MCCS_Version_Id   mccs_version_id;

   // Commands
   Cmd_Id_Type            cmd_id;
   GArray *               setvcp_values;

   // Behavior Modification
   uint8_t                explicit_i2c_source_addr;
   int                    edid_read_size;
   gchar **               ddc_disabled;

   // Display Selection
   Display_Identifier*    pdid;
   Display_Selector*      dsel;   // for future use
   Bit_Set_32             ignored_hiddevs;
   uint8_t                ignored_usb_vid_pid_ct;
   uint32_t               ignored_usb_vid_pids[IGNORED_VID_PID_MAX];

   // Feature Selection
   Feature_Set_Ref*       fref;

   // Performance and Tuning
   Cache_Types            cache_types;
   Cache_Types            discarded_cache_types;
   uint16_t               max_tries[3];
   float                  sleep_multiplier;
   float                  min_dynamic_multiplier;
   DDCA_Stats_Type        stats_types;
   int16_t                i2c_bus_check_async_min;
   int16_t                ddc_check_async_min;
   DDC_Watch_Mode         watch_mode;
   uint16_t               xevent_watch_loop_millisec;
   uint16_t               poll_watch_loop_millisec;

   // Tracing and logging
   DDCA_Trace_Group       traced_groups;
   gchar **               traced_files;
   gchar **               traced_functions;
   gchar **               traced_calls;
   gchar **               traced_api_calls;
   gchar **               backtraced_functions;
   char *                 trace_destination;
   DDCA_Syslog_Level      syslog_level;

   // Other Development
   char *                 failsim_control_fn;

   // Options for temporary use
   int                    i1;         // for temporary use
   int                    i2;         // for temporary use
   int                    i3;         // for temporary use
   int                    i4;         // for temporary use
   int                    i5;         // for temporary use
   int                    i6;         // for temporary use
   int                    i7;         // for temporary use
   int                    i8;         // for temporary use
   int                    i9;         // for temporary use
   int                   i10;         // for temporary use
   int                   i11;         // for temporary use
   int                   i12;         // for temporary use
   int                   i13;         // for temporary use
   int                   i14;         // for temporary use
   int                   i15;         // for temporary use
   int                   i16;         // for temporary use
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
