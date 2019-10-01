/** @file parsed_cmd.c
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
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


/* Allocates new Parsed_Cmd data structure, sets default values.
 *
 * Returns:
 *    initialized ParsedCmd
 */
Parsed_Cmd *  new_parsed_cmd() {
   // DBGMSG("Starting. file=%s", __FILE__);
   Parsed_Cmd * parsed_cmd = calloc(1, sizeof(Parsed_Cmd));
   memcpy(parsed_cmd->marker, PARSED_CMD_MARKER, 4);
   // n. all flags are false, byte values 0, integers 0, pointers NULL because of calloc
   // parsed_cmd->output_level = OL_DEFAULT;
   parsed_cmd->output_level = DDCA_OL_NORMAL;
   parsed_cmd->i1 = -1;    // if set, values should be >= 0
   // parsed_cmd->nodetect = true;
   parsed_cmd->flags |= CMD_FLAG_NODETECT;
   return parsed_cmd;
}


// Debugging function
void dbgrpt_parsed_cmd(Parsed_Cmd * parsed_cmd, int depth) {
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_structure_loc("Parsed_Cmd", parsed_cmd, depth);
   rpt_int_as_hex( "cmd_id", NULL,  parsed_cmd->cmd_id,               d1);

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
   snprintf(buf,20, "%d,%d,%d", parsed_cmd->max_tries[0], parsed_cmd->max_tries[1], parsed_cmd->max_tries[2] );
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
   rpt_str ("MCCS version spec", NULL, format_vspec(parsed_cmd->mccs_vspec),                  d1);
   rpt_str ("MCCS version id",   NULL, vcp_version_id_name(parsed_cmd->mccs_version_id),      d1);

#ifdef FUTURE
   char * interpreted_flags = vnt_interpret_flags(parsed_cmd->flags, cmd_flag_table, false, ", ");
   rpt_str("flags", NULL, interpreted_flags, d1);
   free(interpreted_flags);
#endif

   rpt_vstring(d1, "sleep multiplier: %9.1f", parsed_cmd->sleep_multiplier);
   rpt_int("i1",                 NULL, parsed_cmd->i1,                            d1);
}


void free_parsed_cmd(Parsed_Cmd * parsed_cmd) {
   bool debug = false;
   DBGMSF(debug, "Starting.  parsed_cmd=%p", parsed_cmd);
   assert ( memcmp(parsed_cmd->marker,PARSED_CMD_MARKER,4) == 0);
   int ndx = 0;
   for (; ndx < parsed_cmd->argct; ndx++)
      free(parsed_cmd->args[ndx]);
   if (parsed_cmd->pdid)
      free_display_identifier(parsed_cmd->pdid);

   free(parsed_cmd->failsim_control_fn);
   free(parsed_cmd->fref);
   ntsa_free(parsed_cmd->traced_files, true);
   ntsa_free(parsed_cmd->traced_functions, true);

   parsed_cmd->marker[3] = 'x';
   free(parsed_cmd);
   DBGMSF(debug, "Done");
}
