/** @file parsed_cmd.c
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "base/core.h"

#include "cmdline/parsed_cmd.h"

//
// Parsed_Cmd data structure
//

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
      VNT_END
};


/** Returns the symbolic name for a Cmd_Id_Type value */
const char *  cmdid_name(Cmd_Id_Type id) {
   return vnt_name(cmd_id_table, id);
}

#ifdef FUTURE
Value_Name_Table cmd_flag_table = {
      VNT(CMD_FLAG_DDCDATA,          "report DDC errors"),
      VNT(CMD_FLAG_FORCE,            "ignore certain errors"),
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
   parsed_cmd->i1 = -1;               // if set, values are >= 0
   // parsed_cmd->nodetect = true;
   parsed_cmd->flags |= CMD_FLAG_NODETECT;
   parsed_cmd->setvcp_values = g_array_new(false,         // not null-terminated
                                           true,          // clear to 0's
                                           sizeof(Parsed_Setvcp_Args));
   g_array_set_clear_func(parsed_cmd->setvcp_values, destroy_parsed_setvcp_value);
   return parsed_cmd;
}


/** Frees a #Parsed_Cmd data structure
 *  \param parsed_cmd pointer to instance to free
 */
void free_parsed_cmd(Parsed_Cmd * parsed_cmd) {
   bool debug = false;
   DBGMSF(debug, "Starting.  parsed_cmd=%p", parsed_cmd);
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
   g_array_free(parsed_cmd->setvcp_values, true);

   parsed_cmd->marker[3] = 'x';
   free(parsed_cmd);
   DBGMSF(debug, "Done");
}


/** Dumps the #Parsed_Command data structure
 *  \param  parsed_cmd  pointer to instance
 *  \param  depth       logical indentation depth
 */
void dbgrpt_parsed_cmd(Parsed_Cmd * parsed_cmd, int depth) {
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_structure_loc("Parsed_Cmd", parsed_cmd, depth);
   rpt_str("raw_command",       NULL, parsed_cmd->raw_command,        d1);
   rpt_int_as_hex( "cmd_id",    NULL, parsed_cmd->cmd_id,             d1);

   rpt_structure_loc("pdid", parsed_cmd->pdid,                        d1);
   if (parsed_cmd->pdid)
       dbgrpt_display_identifier(parsed_cmd->pdid,                    d2);

   rpt_structure_loc("fref", parsed_cmd->fref,                        d1);
   if (parsed_cmd->fref)
       dbgrpt_feature_set_ref(parsed_cmd->fref,                       d2);

   rpt_int_as_hex(
            "stats",            NULL, parsed_cmd->stats_types,                       d1);
   rpt_bool("ddcdata",          NULL, parsed_cmd->flags & CMD_FLAG_DDCDATA,          d1);
   rpt_str( "output_level",     NULL, output_level_name(parsed_cmd->output_level),   d1);
   rpt_bool("force_slave_addr", NULL, parsed_cmd->flags & CMD_FLAG_FORCE_SLAVE_ADDR, d1);
   rpt_bool("verify_setvcp",    NULL, parsed_cmd->flags & CMD_FLAG_VERIFY,           d1);
   rpt_bool("timestamp_trace",  NULL, parsed_cmd->flags & CMD_FLAG_TIMESTAMP_TRACE,  d1);
   rpt_int_as_hex(
            "traced_groups",    NULL,  parsed_cmd->traced_groups,                    d1);
   if (parsed_cmd->traced_functions) {
      char * joined = g_strjoinv(", ", parsed_cmd->traced_functions);
      rpt_str("traced_functions", NULL, joined, d1);
      free(joined);
   }
   else
      rpt_str("traced_functions", NULL, "none", d1);
   if (parsed_cmd->traced_files) {
      char * joined = g_strjoinv(", ", parsed_cmd->traced_files);
      rpt_str("traced_files", NULL, joined, d1);
      free(joined);
   }
   else
      rpt_str("traced_files", NULL, "none", d1);

   rpt_int( "argct",       NULL,  parsed_cmd->argct, d1);
   int ndx = 0;
   for (ndx = 0; ndx < parsed_cmd->argct; ndx++) {
      printf("   argument %d:  %s\n", ndx, parsed_cmd->args[ndx]);
   }
   char buf[20];
   g_snprintf(buf,30, "%d,%d,%d", parsed_cmd->max_tries[0], parsed_cmd->max_tries[1],
                                     parsed_cmd->max_tries[2] );
   rpt_str("raw_command",        NULL, parsed_cmd->raw_command,                               d1);
   rpt_str("max_retries",        NULL, buf,                                                   d1);

   rpt_bool("enable_failure_simulation", NULL, parsed_cmd->flags & CMD_FLAG_ENABLE_FAILSIM,   d1);
   rpt_str("failsim_control_fn", NULL, parsed_cmd->failsim_control_fn,                        d1);
   rpt_bool("nodetect",          NULL, parsed_cmd->flags & CMD_FLAG_NODETECT,                 d1);
   rpt_bool("async",             NULL, parsed_cmd->flags & CMD_FLAG_ASYNC,                    d1);
   rpt_bool("report_freed_exceptions", NULL, parsed_cmd->flags & CMD_FLAG_REPORT_FREED_EXCP,  d1);
   rpt_bool("force",             NULL, parsed_cmd->flags & CMD_FLAG_FORCE,                    d1);
   rpt_bool("notable",           NULL, parsed_cmd->flags & CMD_FLAG_NOTABLE,                  d1);
   rpt_bool("rw only",           NULL, parsed_cmd->flags & CMD_FLAG_RW_ONLY,                  d1);
   rpt_bool("ro only",           NULL, parsed_cmd->flags & CMD_FLAG_RO_ONLY,                  d1);
   rpt_bool("wo only",           NULL, parsed_cmd->flags & CMD_FLAG_WO_ONLY,                  d1);
   rpt_bool("show unsupported",  NULL, parsed_cmd->flags & CMD_FLAG_SHOW_UNSUPPORTED,         d1);
   rpt_bool("enable udf",        NULL, parsed_cmd->flags & CMD_FLAG_ENABLE_UDF,               d1);
   rpt_bool("enable usb",        NULL, parsed_cmd->flags & CMD_FLAG_ENABLE_USB,               d1);
   rpt_bool("timestamp prefix:", NULL, parsed_cmd->flags & CMD_FLAG_TIMESTAMP_TRACE,          d1);
   rpt_bool("thread id prefix:", NULL, parsed_cmd->flags & CMD_FLAG_THREAD_ID_TRACE,          d1);
   rpt_bool("enable cached capabilities:",
                                 NULL, parsed_cmd->flags & CMD_FLAG_ENABLE_CACHED_CAPABILITIES, d1);
   rpt_bool("ignore cached capabilities:",
                                 NULL, parsed_cmd->flags & CMD_FLAG_IGNORE_CACHED_CAPABILITIES, d1);
// rpt_bool("clear persistent cache:",
//                                 NULL, parsed_cmd->flags & CMD_FLAG_CLEAR_PERSISTENT_CACHE, d1);
   rpt_str ("MCCS version spec", NULL, format_vspec(parsed_cmd->mccs_vspec),                  d1);
// rpt_str ("MCCS version id",   NULL, vcp_version_id_name(parsed_cmd->mccs_version_id),      d1);

#ifdef FUTURE
   char * interpreted_flags = vnt_interpret_flags(parsed_cmd->flags, cmd_flag_table, false, ", ");
   rpt_str("flags", NULL, interpreted_flags, d1);
   free(interpreted_flags);
#endif

   rpt_vstring(d1, "sleep multiplier: %9.1f", parsed_cmd->sleep_multiplier);
   rpt_bool("timeout I2C IO:",   NULL, parsed_cmd->flags & CMD_FLAG_TIMEOUT_I2C_IO,          d1);
   rpt_bool("reduce sleeps:",    NULL, parsed_cmd->flags & CMD_FLAG_REDUCE_SLEEPS,           d1);
   rpt_bool("defer sleeps:",     NULL, parsed_cmd->flags & CMD_FLAG_DEFER_SLEEPS,            d1);
   rpt_bool("dynamic_sleep_adjustment:", NULL, parsed_cmd->flags & CMD_FLAG_DSA,             d1);
   rpt_bool("per_thread_stats:", NULL, parsed_cmd->flags & CMD_FLAG_PER_THREAD_STATS,        d1);
   rpt_bool("x52 not fifo:",     NULL, parsed_cmd->flags & CMD_FLAG_X52_NO_FIFO,             d1);
   rpt_int("setvcp value count:",NULL, parsed_cmd->setvcp_values->len,                       d1);
   for (int ndx = 0; ndx < parsed_cmd->setvcp_values->len; ndx++) {
      Parsed_Setvcp_Args * elem = &g_array_index(parsed_cmd->setvcp_values, Parsed_Setvcp_Args, ndx);
      rpt_vstring(d2, "feature_code: 0x%02x, relative: %s, value: %s",
                      elem->feature_code,
                      setvcp_value_type_name(elem->feature_value_type),
                      elem->feature_value);
   }
   rpt_int( "edid_read_size:",   NULL, parsed_cmd->edid_read_size,                d1);
   rpt_int( "i1",                NULL, parsed_cmd->i1,                            d1);
   rpt_bool("f1",                NULL, parsed_cmd->flags & CMD_FLAG_F1,           d1);
   rpt_bool("f2",                NULL, parsed_cmd->flags & CMD_FLAG_F2,           d1);
   rpt_bool("f3",                NULL, parsed_cmd->flags & CMD_FLAG_F3,           d1);
   rpt_bool("f4",                NULL, parsed_cmd->flags & CMD_FLAG_F4,           d1);
   rpt_bool("f5",                NULL, parsed_cmd->flags & CMD_FLAG_F5,           d1);
   rpt_bool("f6",                NULL, parsed_cmd->flags & CMD_FLAG_F6,           d1);
}
