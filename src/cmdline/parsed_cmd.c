/* parsed_cmd.c
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/report_util.h"

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
   parsed_cmd->sleep_strategy = -1;    // use default
   return parsed_cmd;
}


// Debugging function
void report_parsed_cmd(Parsed_Cmd * parsed_cmd, int depth) {
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_structure_loc("Parsed_Cmd", parsed_cmd, depth);
   rpt_int_as_hex( "cmd_id", NULL,  parsed_cmd->cmd_id,                 d1);

   rpt_structure_loc("pdid", parsed_cmd->pdid,                        d1);
   if (parsed_cmd->pdid)
       report_display_identifier(parsed_cmd->pdid,                    d2);

   rpt_structure_loc("fref", parsed_cmd->fref,                        d1);
   if (parsed_cmd->fref)
       report_feature_set_ref(parsed_cmd->fref,                       d2);

   rpt_int_as_hex(
            "stats",       NULL,  parsed_cmd->stats_types,            d1);
   rpt_bool("ddcdata",     NULL,  parsed_cmd->ddcdata,                d1);
#ifdef OLD
   rpt_str( "msg_level",   NULL,  msg_level_name(parsed_cmd->msg_level), d1);
   rpt_bool("prog output", NULL,  parsed_cmd->programmatic_output,    d1);
#endif
   rpt_str( "output_level",NULL,  output_level_name(parsed_cmd->output_level), d1);
   rpt_bool("force",       NULL,  parsed_cmd->force,                  d1);
   rpt_bool("force_slave_addr", NULL, parsed_cmd->force_slave_addr,   d1);
   rpt_bool("show_unsupported", NULL, parsed_cmd->show_unsupported,   d1);
   rpt_bool("verify_setvcp",    NULL, parsed_cmd->verify_setvcp,       d1);
   rpt_bool("timestamp_trace",  NULL, parsed_cmd->timestamp_trace,    d1);
   rpt_int_as_hex(
            "trace",       NULL,  parsed_cmd->trace,                  d1);
   rpt_int( "argct",       NULL,  parsed_cmd->argct,                  d1);
   int ndx = 0;
   for (ndx = 0; ndx < parsed_cmd->argct; ndx++) {
      printf("  argument %d:  %s\n", ndx, parsed_cmd->args[ndx]);
   }
   char buf[20];
   snprintf(buf,20, "%d,%d,%d", parsed_cmd->max_tries[0], parsed_cmd->max_tries[1], parsed_cmd->max_tries[2] );
   rpt_str("max_retries",  NULL, buf, d1);
   rpt_int("sleep_stragegy", NULL, parsed_cmd->sleep_strategy,       d1);
   rpt_bool("enable_failure_simulation", NULL, parsed_cmd->enable_failure_simulation, d1);
   rpt_str("failsim_control_fn", NULL, parsed_cmd->failsim_control_fn, d1);

#ifdef FUTURE
   char * interpreted_flags = vnt_interpret_flags(parsed_cmd->flags, cmd_flag_table, false, ", ");
   rpt_str("flags", NULL, interpreted_flags, d1);
   free(interpreted_flags);
#endif

}


void free_parsed_cmd(Parsed_Cmd * parsed_cmd) {
   assert ( memcmp(parsed_cmd->marker,PARSED_CMD_MARKER,4) == 0);
   int ndx = 0;
   for (; ndx < parsed_cmd->argct; ndx++)
      free(parsed_cmd->args[ndx]);
   if (parsed_cmd->pdid)
      free_display_identifier(parsed_cmd->pdid);
   parsed_cmd->marker[3] = 'x';
   free(parsed_cmd);
}
