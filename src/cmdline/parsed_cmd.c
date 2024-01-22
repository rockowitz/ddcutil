/** @file parsed_cmd.c
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "util/data_structures.h"
#include "util/glib_string_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "usb/usb_base.h"

#include "base/core.h"
#include "base/parms.h"

#include "cmdline/parsed_cmd.h"

//
// Parsed_Cmd data structure
//

const char *  parser_mode_name(Parser_Mode mode) {
   // use switch to force compilation error if a mode is added but not named
   char * name = NULL;
   switch(mode) {
   case MODE_DDCUTIL:    name = "ddcutil";    break;
   case MODE_LIBDDCUTIL: name = "libddcutil"; break;
   }
   return name;
}


// Must be kept in sync with Cmd_Id_Type
Value_Name_Table cmd_id_table = {
      VNT(CMDID_NONE          , "none"),
      VNT(CMDID_DETECT        , "detect"),
      VNT(CMDID_CAPABILITIES  , "capabilities"),
      VNT(CMDID_GETVCP        , "getvcp"),
      VNT(CMDID_SETVCP        , "setvcp"),
      VNT(CMDID_LISTVCP       , "listvcp"),
      VNT(CMDID_TESTCASE      , "testcase"),
      VNT(CMDID_LISTTESTS     , "listtests"),
      VNT(CMDID_LOADVCP       , "loadvcp"),
      VNT(CMDID_DUMPVCP       , "dumpvcp"),
   #ifdef ENABLE_ENVCMDS
      VNT(CMDID_INTERROGATE   , "interrogate"),
      VNT(CMDID_ENVIRONMENT   , "environment"),
      VNT(CMDID_USBENV        , "usbenvironment"),
   #endif
      VNT(CMDID_VCPINFO       ,  "vcpinfo"),
      VNT(CMDID_READCHANGES   ,  "watch"),
      VNT(CMDID_CHKUSBMON     ,  "chkusbmon"),
      VNT(CMDID_PROBE         ,  "probe"),
      VNT(CMDID_SAVE_SETTINGS ,  "save settings"),
      VNT(CMDID_DISCARD_CACHE ,  "discard cache"),
      VNT(CMDID_LIST_RTTI     ,  "traceable functions"),
      VNT(CMDID_C1            ,  "c1"),
      VNT(CMDID_C2            ,  "c2"),
      VNT(CMDID_C3            ,  "c3"),
      VNT(CMDID_C4            ,  "c4"),
      VNT_END
};


/** Returns the symbolic name for a Cmd_Id_Type value */
const char *  cmdid_name(Cmd_Id_Type id) {
   return vnt_name(cmd_id_table, id);
}

#ifdef FUTURE
Value_Name_Table cmd_flag_table = {
      VNT(CMD_FLAG_DDCDATA,          "report DDC errors"),
      VNT(CMD_FLAG_FORCE_UNRECOGNIZED_VCP_CODE,            "ignore certain errors"),
      VNT(CMD_FLAG_FORCE_SLAVE_ADDR, "force slave address setting"),
      VNT(CMD_FLAG_TIMESTAMP_TRACE,  "include timestamp on trace messages"),
      VNT(CMD_FLAG_SHOW_UNSUPPORTED, "show unsupported VCP features"),
      VNT(CMD_FLAG_ENABLE_FAILSIM,   "enable failure simulation"),
      VNT(CMD_FLAG_VERIFY,           "read VCP features after setting them"),
      // etc
};
#endif


/** Returns the symbolic name for a Setvcp_Value_Type value */
const char * setvcp_value_type_name(Setvcp_Value_Type value_type)
{
   char * names[] = {"VALUE_TYPE_ABSOLUTE",
                     "VALUE_TYPE_RELAIIVE_PLUS",
                     "VALUE_TYPE_RELATIVE_MINUS"};
   return names[value_type];
}


/** Called by g_array_free()
 *  Conforms to GDestroyNotify():
 *
 *  \param data  pointer to Parsed_Setup_Value to clear
 */
static void destroy_parsed_setvcp_value(gpointer data) {
   Parsed_Setvcp_Args * psv = (Parsed_Setvcp_Args *) data;
   free(psv->feature_value);
   memset(psv, 0, sizeof(Parsed_Setvcp_Args));
}

/** Allocates and initializes a #Parsed_Cmd data structure
 *
 *  \return  pointer to initialized #Parsed_Cmd
 */
Parsed_Cmd *  new_parsed_cmd() {
   // DBGMSG("Starting. file=%s", __FILE__);
   Parsed_Cmd * parsed_cmd = calloc(1, sizeof(Parsed_Cmd));
   memcpy(parsed_cmd->marker, PARSED_CMD_MARKER, 4);
   // n. all flags are false, byte values 0, integers 0, pointers NULL because of calloc
   // parsed_cmd->output_level = OL_DEFAULT;
   parsed_cmd->output_level = DDCA_OL_NORMAL;
   parsed_cmd->edid_read_size = -1;   // if set, values are >= 0
   parsed_cmd->sleep_multiplier = 1.0;
   parsed_cmd->min_dynamic_multiplier = -1.0;
   parsed_cmd->i2c_bus_check_async_min = -1;
   parsed_cmd->ddc_check_async_min = -1;
   parsed_cmd->i1 = -1;               // if set, values are >= 0
#ifdef OLD
   parsed_cmd->flags |= CMD_FLAG_NODETECT;
#endif
   parsed_cmd->setvcp_values = g_array_new(false,         // not null-terminated
                                           true,          // clear to 0's
                                           sizeof(Parsed_Setvcp_Args));
   g_array_set_clear_func(parsed_cmd->setvcp_values, destroy_parsed_setvcp_value);
   if (DEFAULT_ENABLE_UDF)
      parsed_cmd->flags |= CMD_FLAG_ENABLE_UDF;
#ifdef ENABLE_USB
   if (DEFAULT_ENABLE_USB)
      parsed_cmd->flags |= CMD_FLAG_ENABLE_USB;
#endif
   if (DEFAULT_ENABLE_CACHED_CAPABILITIES)
      parsed_cmd->flags |= CMD_FLAG_ENABLE_CACHED_CAPABILITIES;
   return parsed_cmd;
}


/** Frees a #Parsed_Cmd data structure
 *  \param parsed_cmd pointer to instance to free
 */
void free_parsed_cmd(Parsed_Cmd * parsed_cmd) {
   bool debug = false;
   DBGMSF(debug, "Starting.  parsed_cmd=%p", parsed_cmd);
      if (parsed_cmd) {
      assert ( memcmp(parsed_cmd->marker,PARSED_CMD_MARKER,4) == 0);
      int ndx = 0;
      for (; ndx < parsed_cmd->argct; ndx++)
         free(parsed_cmd->args[ndx]);
      if (parsed_cmd->pdid)
         free_display_identifier(parsed_cmd->pdid);
      free(parsed_cmd->raw_command);
      free(parsed_cmd->failsim_control_fn);
      free(parsed_cmd->fref);
      ntsa_free(parsed_cmd->traced_files, true);
      ntsa_free(parsed_cmd->traced_functions, true);
      ntsa_free(parsed_cmd->traced_calls, true);
      ntsa_free(parsed_cmd->traced_api_calls, true);
      g_array_free(parsed_cmd->setvcp_values, true);
      free(parsed_cmd->s1);
      free(parsed_cmd->s2);
      free(parsed_cmd->s3);
      free(parsed_cmd->s4);

      parsed_cmd->marker[3] = 'x';
      free(parsed_cmd);
   }
   DBGMSF(debug, "Done");
}

static void
dbgrpt_ntsa(int depth, char * title, gchar** values) {
   if (values) {
      char * joined = g_strjoinv(", ", values);
      rpt_str(title, NULL, joined, depth);
      free(joined);
   }
   else
      rpt_str(title, NULL, "none", depth);
}


#define RPT_CMDFLAG(_desc, _flag, _depth) \
   rpt_str(_desc, NULL, SBOOL(parsed_cmd->flags & _flag), _depth)

/** Dumps the #Parsed_Command data structure
 *  \param  parsed_cmd  pointer to instance
 *  \param  depth       logical indentation depth
 */
void dbgrpt_parsed_cmd(Parsed_Cmd * parsed_cmd, int depth) {
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_structure_loc("Parsed_Cmd", parsed_cmd, depth);

   if (parsed_cmd) {
      rpt_nl();
      rpt_label(depth, "General");
      rpt_str("raw_command",       NULL, parsed_cmd->raw_command,        d1);
      rpt_str("parser mode",       NULL, parser_mode_name(parsed_cmd->parser_mode), d1);
      rpt_int_as_hex( "cmd_id",    NULL, parsed_cmd->cmd_id,             d1);
      rpt_int( "argct",       NULL,  parsed_cmd->argct, d1);
      int ndx = 0;
      for (ndx = 0; ndx < parsed_cmd->argct; ndx++) {
         printf("   argument %d:  %s\n", ndx, parsed_cmd->args[ndx]);
      }
      rpt_str( "output_level",     NULL, output_level_name(parsed_cmd->output_level),   d1);
      rpt_str ("MCCS version spec", NULL, format_vspec(parsed_cmd->mccs_vspec),                  d1);
   // rpt_str ("MCCS version id",   NULL, vcp_version_id_name(parsed_cmd->mccs_version_id),      d1);

      rpt_nl();
      rpt_label(depth, "Commands");
      rpt_int("setvcp value count:",NULL, parsed_cmd->setvcp_values->len,                       d1);
           for (int ndx = 0; ndx < parsed_cmd->setvcp_values->len; ndx++) {
              Parsed_Setvcp_Args * elem = &g_array_index(parsed_cmd->setvcp_values, Parsed_Setvcp_Args, ndx);
              rpt_vstring(d2, "feature_code: 0x%02x, relative: %s, value: %s",
                              elem->feature_code,
                              setvcp_value_type_name(elem->feature_value_type),
                              elem->feature_value);
           }

      rpt_nl();
      rpt_label(depth, "Behavior modification");
      RPT_CMDFLAG("i2c source addr set", CMD_FLAG_EXPLICIT_I2C_SOURCE_ADDR, d1);
      if (parsed_cmd->flags & CMD_FLAG_EXPLICIT_I2C_SOURCE_ADDR)
         rpt_vstring(d2, "explicit_i2c_source_addr:    0x%02x", parsed_cmd->explicit_i2c_source_addr);
      rpt_int( "edid_read_size",   NULL, parsed_cmd->edid_read_size,                d1);

      rpt_bool("force_slave_addr", NULL, parsed_cmd->flags & CMD_FLAG_FORCE_SLAVE_ADDR, d1);
      rpt_bool("verify_setvcp",    NULL, parsed_cmd->flags & CMD_FLAG_VERIFY,           d1);
//    rpt_bool("async",             NULL, parsed_cmd->flags & CMD_FLAG_ASYNC,                    d1);
      rpt_bool("force",             NULL, parsed_cmd->flags & CMD_FLAG_FORCE_UNRECOGNIZED_VCP_CODE,                    d1);

      rpt_bool("enable udf",        NULL, parsed_cmd->flags & CMD_FLAG_ENABLE_UDF,               d1);
      rpt_bool("x52 not fifo:",     NULL, parsed_cmd->flags & CMD_FLAG_X52_NO_FIFO,             d1);
        rpt_bool("i2c_io_fileio",    NULL, parsed_cmd->flags & CMD_FLAG_I2C_IO_FILEIO,d1);
        rpt_bool("i2c_io_ioctl",     NULL, parsed_cmd->flags & CMD_FLAG_I2C_IO_IOCTL, d1);
        RPT_CMDFLAG("heuristicly detect unsupported features", CMD_FLAG_HEURISTIC_UNSUPPORTED_FEATURES, d1);
        rpt_bool("quick",             NULL, parsed_cmd->flags & CMD_FLAG_QUICK,                   d1);

        RPT_CMDFLAG("watch hotplug events", CMD_FLAG_WATCH_DISPLAY_HOTPLUG_EVENTS, d1);

      rpt_nl();
      rpt_label(depth, "Display Selection");
#ifdef ENABLE_USB
      rpt_bool("enable usb",        NULL, parsed_cmd->flags & CMD_FLAG_ENABLE_USB,               d1);
#endif
      rpt_structure_loc("pdid", parsed_cmd->pdid,                        d1);
      if (parsed_cmd->pdid)
          dbgrpt_display_identifier(parsed_cmd->pdid,                    d2);
      char buf2[BIT_SET_32_MAX+1];
      bs32_to_bitstring(parsed_cmd->ignored_hiddevs, buf2, BIT_SET_32_MAX+1);
      rpt_vstring(d1, "ignored_hiddevs                                          : 0x%08x = |%s|",
            parsed_cmd->ignored_hiddevs, buf2);
      rpt_int( "ignored_vid_pid_ct", NULL, parsed_cmd->ignored_usb_vid_pid_ct, d1);
      for (int ndx = 0; ndx < parsed_cmd->ignored_usb_vid_pid_ct; ndx++) {
         Vid_Pid_Value v = parsed_cmd->ignored_usb_vid_pids[ndx];
         rpt_vstring(d1, "ignored_vid_pids[%d]                                      : %04x:%04x",
                     ndx, VID_PID_VALUE_TO_VID(v), VID_PID_VALUE_TO_PID(v) );
      }


      rpt_nl();
      rpt_label(depth, "Feature Selection");
      rpt_structure_loc("fref", parsed_cmd->fref,                        d1);
      if (parsed_cmd->fref)
          dbgrpt_feature_set_ref(parsed_cmd->fref,                       d2);
      rpt_bool("notable",           NULL, parsed_cmd->flags & CMD_FLAG_NOTABLE,                  d1);
      rpt_bool("rw only",           NULL, parsed_cmd->flags & CMD_FLAG_RW_ONLY,                  d1);
      rpt_bool("ro only",           NULL, parsed_cmd->flags & CMD_FLAG_RO_ONLY,                  d1);
      rpt_bool("wo only",           NULL, parsed_cmd->flags & CMD_FLAG_WO_ONLY,                  d1);
      rpt_bool("show unsupported",  NULL, parsed_cmd->flags & CMD_FLAG_SHOW_UNSUPPORTED,         d1);

#ifdef REF

   // Tracing and logging
   DDCA_Trace_Group       traced_groups;
   gchar **               traced_files;
   gchar **               traced_functions;
   gchar **               traced_calls;
   gchar **               traced_api_calls;
   char *                 trace_destination;
   DDCA_Syslog_Level      syslog_level;

   // Other Development
   char *                 failsim_control_fn;

#endif

   rpt_nl();
      rpt_label(depth, "Performance and Tuning");
      rpt_bool("enable cached capabilities",
                                 NULL, parsed_cmd->flags & CMD_FLAG_ENABLE_CACHED_CAPABILITIES, d1);
      rpt_bool("enable cached displays",
                                 NULL, parsed_cmd->flags & CMD_FLAG_ENABLE_CACHED_DISPLAYS,   d1);
      rpt_vstring(d1, "cache types:              0x%02x", parsed_cmd->cache_types);
      RPT_CMDFLAG("discard caches", CMD_FLAG_DISCARD_CACHES, d1);
      rpt_vstring(d1, "discarded cache types:    0x%02x", parsed_cmd->discarded_cache_types);
   // rpt_bool("clear persistent cache:",
   //                               NULL, parsed_cmd->flags & CMD_FLAG_CLEAR_PERSISTENT_CACHE,   d1);
      rpt_vstring(d1, "sleep multiplier                                         : %.3f", parsed_cmd->sleep_multiplier);
      rpt_vstring(d1, "min dynamic sleep multiplier                             : %.3f", parsed_cmd->min_dynamic_multiplier);
      rpt_bool("explicit sleep multiplier", NULL, parsed_cmd->flags & CMD_FLAG_EXPLICIT_SLEEP_MULTIPLIER, d1);
#ifdef OLD
      rpt_bool("timeout I2C IO:",   NULL, parsed_cmd->flags & CMD_FLAG_TIMEOUT_I2C_IO,          d1);
      rpt_bool("reduce sleeps:",    NULL, parsed_cmd->flags & CMD_FLAG_REDUCE_SLEEPS,           d1);
#endif
      rpt_bool("defer sleeps",      NULL, parsed_cmd->flags & CMD_FLAG_DEFER_SLEEPS,            d1);
      rpt_bool("dsa2 enabled",      NULL, parsed_cmd->flags & CMD_FLAG_DSA2,                    d1);
      rpt_int("i2c_bus_check_async_min", NULL, parsed_cmd->i2c_bus_check_async_min,             d1);
      rpt_int("ddc_check_async_min", NULL, parsed_cmd->ddc_check_async_min,                     d1);


      rpt_bool("verbose stats:", NULL, parsed_cmd->flags & CMD_FLAG_VERBOSE_STATS,      d1);
      RPT_CMDFLAG("internal stats", CMD_FLAG_INTERNAL_STATS, d1);

      rpt_int_as_hex(
               "stats",            NULL, parsed_cmd->stats_types,                       d1);
      rpt_bool("stats to syslog only", NULL, parsed_cmd->flags & CMD_FLAG_STATS_TO_SYSLOG, d1);
      char buf[30];
      g_snprintf(buf,30, "%d,%d,%d", parsed_cmd->max_tries[0], parsed_cmd->max_tries[1],
                                        parsed_cmd->max_tries[2] );
      rpt_str("max_retries",        NULL, buf,                                                   d1);
      rpt_bool("profile API",       NULL, parsed_cmd->flags & CMD_FLAG_PROFILE_API,             d1);

      rpt_nl();
      rpt_label(depth, "Tracing and Logging");
      rpt_bool("timestamp_trace",  NULL, parsed_cmd->flags & CMD_FLAG_TIMESTAMP_TRACE,  d1);
      rpt_int_as_hex(
               "traced_groups",    NULL,  parsed_cmd->traced_groups,                    d1);
      dbgrpt_ntsa(d1, "traced_functions", parsed_cmd->traced_functions);
      dbgrpt_ntsa(d1, "traced_files", parsed_cmd->traced_files);
      dbgrpt_ntsa(d1, "traced_api_calls", parsed_cmd->traced_api_calls);
      dbgrpt_ntsa(d1, "traced_calls", parsed_cmd->traced_calls);
      rpt_str ("library trace file", NULL, parsed_cmd->trace_destination,           d1);
      rpt_bool("trace to syslog only", NULL, parsed_cmd->flags & CMD_FLAG_TRACE_TO_SYSLOG_ONLY, d1);

      rpt_str("syslog_level",      NULL, syslog_level_name(parsed_cmd->syslog_level), d1);
      rpt_bool("timestamp prefix:", NULL, parsed_cmd->flags & CMD_FLAG_TIMESTAMP_TRACE,          d1);
      rpt_bool("walltime prefix:",  NULL, parsed_cmd->flags & CMD_FLAG_WALLTIME_TRACE,           d1);
      rpt_bool("thread id prefix:", NULL, parsed_cmd->flags & CMD_FLAG_THREAD_ID_TRACE,          d1);
      rpt_bool("process id prefix:",NULL, parsed_cmd->flags & CMD_FLAG_PROCESS_ID_TRACE,         d1);

      rpt_nl();
      rpt_label(depth, "Other Development");
      rpt_bool("enable_failure_simulation", NULL, parsed_cmd->flags & CMD_FLAG_ENABLE_FAILSIM,   d1);
      rpt_str("failsim_control_fn", NULL, parsed_cmd->failsim_control_fn,                        d1);
      rpt_bool("mock data",         NULL, parsed_cmd->flags & CMD_FLAG_MOCK,                    d1);
      RPT_CMDFLAG("simulate Null Msg indicates unsupported", CMD_FLAG_NULL_MSG_INDICATES_UNSUPPORTED_FEATURE, d1);
      RPT_CMDFLAG("skip ddc checks",      CMD_FLAG_SKIP_DDC_CHECKS, d1);
      RPT_CMDFLAG("async I2C bus checks", CMD_FLAG_ASYNC_I2C_CHECK, d1);
      RPT_CMDFLAG("enable_flock",         CMD_FLAG_FLOCK, d1);

      rpt_nl();
      rpt_label(depth, "Unsorted");

      rpt_bool("ddcdata",          NULL, parsed_cmd->flags & CMD_FLAG_DDCDATA,          d1);


#ifdef OLD
      rpt_bool("nodetect",          NULL, parsed_cmd->flags & CMD_FLAG_NODETECT,                 d1);
#endif

      rpt_bool("report_freed_exceptions", NULL, parsed_cmd->flags & CMD_FLAG_REPORT_FREED_EXCP,  d1);
      rpt_bool("show settings",     NULL, parsed_cmd->flags & CMD_FLAG_SHOW_SETTINGS,            d1);

#ifdef FUTURE
      char * interpreted_flags = vnt_interpret_flags(parsed_cmd->flags, cmd_flag_table, false, ", ");
      rpt_str("flags", NULL, interpreted_flags, d1);
      free(interpreted_flags);
#endif




#define RPT_IVAL(_n, _depth) \
   do { \
      rpt_bool("i"#_n" set",  NULL, parsed_cmd->flags2 & CMD_FLAG2_I ## _n ## _SET, _depth); \
      if (parsed_cmd->flags2 & CMD_FLAG2_I ## _n ##_SET) {  \
         rpt_int("i"#_n,                   NULL, parsed_cmd->i ## _n, _depth); \
         rpt_int_as_hex("i" #_n " as hex", NULL, parsed_cmd->i ## _n, _depth); \
      } \
   } \
   while(0)


#ifdef OLD
      rpt_nl();
      rpt_label(depth, "Temporary Utility Variables");
      rpt_bool("i1 set",           NULL, parsed_cmd->flags2 & CMD_FLAG2_I1_SET,       d1);
      if (parsed_cmd->flags2 & CMD_FLAG2_I1_SET) {
         rpt_int( "i1",             NULL, parsed_cmd->i1,                            d1);
         rpt_int_as_hex(
                  "i1 as hex",      NULL, parsed_cmd->i1,                            d1);
      }
      rpt_bool("i2 set",           NULL, parsed_cmd->flags2 & CMD_FLAG2_I2_SET,       d1);
      if (parsed_cmd->flags2 & CMD_FLAG2_I2_SET) {
         rpt_int( "i2",             NULL, parsed_cmd->i2,                            d1);
         rpt_int_as_hex(
                  "i2 as hex",      NULL, parsed_cmd->i2,                            d1);
      }
      rpt_bool("i3 set",           NULL, parsed_cmd->flags2 & CMD_FLAG2_I3_SET,       d1);
      if (parsed_cmd->flags2 & CMD_FLAG2_I3_SET) {
         rpt_int( "i3",             NULL, parsed_cmd->i3,                            d1);
         rpt_int_as_hex(
                  "i3 as hex",      NULL, parsed_cmd->i3,                            d1);
      }
#endif

      RPT_IVAL(1,d1);
      RPT_IVAL(2,d1);
      RPT_IVAL(3,d1);
      RPT_IVAL(4,d1);
      RPT_IVAL(5,d1);
      RPT_IVAL(6,d1);
      RPT_IVAL(7,d1);
      RPT_IVAL(8,d1);

#undef RPT_IVAL

      rpt_bool("fl1 set",           NULL, parsed_cmd->flags & CMD_FLAG_FL1_SET,     d1);
      if (parsed_cmd->flags & CMD_FLAG_FL1_SET)
         rpt_vstring(d1, "fl1                                                      : %.2f", parsed_cmd->fl1);
      rpt_bool("fl2 set",           NULL, parsed_cmd->flags & CMD_FLAG_FL2_SET,     d1);
      if (parsed_cmd->flags & CMD_FLAG_FL2_SET)
         rpt_vstring(d1, "fl2                                                      : %.2f", parsed_cmd->fl2);
      rpt_bool("f1",                NULL, parsed_cmd->flags & CMD_FLAG_F1,           d1);
      rpt_bool("f2",                NULL, parsed_cmd->flags & CMD_FLAG_F2,           d1);
      rpt_bool("f3",                NULL, parsed_cmd->flags & CMD_FLAG_F3,           d1);
      rpt_bool("f4",                NULL, parsed_cmd->flags & CMD_FLAG_F4,           d1);
      rpt_bool("f5",                NULL, parsed_cmd->flags & CMD_FLAG_F5,           d1);
      rpt_bool("f6",                NULL, parsed_cmd->flags & CMD_FLAG_F6,           d1);
      rpt_bool("f7",                NULL, parsed_cmd->flags & CMD_FLAG_F7,           d1);
      rpt_bool("f8",                NULL, parsed_cmd->flags & CMD_FLAG_F8,           d1);
      rpt_bool("f9",                NULL, parsed_cmd->flags & CMD_FLAG_F9,           d1);
      RPT_CMDFLAG("f10", CMD_FLAG_F10, d1);
      RPT_CMDFLAG("f11", CMD_FLAG_F11, d1);
      RPT_CMDFLAG("f12", CMD_FLAG_F12, d1);
      RPT_CMDFLAG("f13", CMD_FLAG_F13, d1);
      RPT_CMDFLAG("f14", CMD_FLAG_F14, d1);
      rpt_str( "s1",                NULL, parsed_cmd->s1,                            d1);
      rpt_str( "s2",                NULL, parsed_cmd->s2,                            d1);
      rpt_str( "s3",                NULL, parsed_cmd->s3,                            d1);
      rpt_str( "s4",                NULL, parsed_cmd->s4,                            d1);
   }
}


#ifdef UNUSED
void dbgrpt_preparsed_cmd(Preparsed_Cmd * ppc, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Preparsed_Cmd", ppc, depth);
   if (ppc) {
      rpt_bool("verbose",         NULL, ppc->verbose,                        d1);
      rpt_bool("noconfig",        NULL, ppc->noconfig,                       d1);
      rpt_str("syslog severity",  NULL, syslog_level_name(ppc->severity), d1);
   }
}
#endif
