/*
 * parsed_cmd.c
 *
 *  Created on: Nov 24, 2015
 *      Author: rock
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "util/report_util.h"

#include "base/displays.h"
#include "base/msg_control.h"

#include "cmdline/parsed_cmd.h"

//
// Parsed_Cmd data structure
//

/* Allocates new Parsed_Cmd data structure, sets default values.
 *
 * Returns:
 *    initialized ParsedCmd
 */
Parsed_Cmd *  new_parsed_cmd() {
   // printf("(%s) Starting. file=%s\n", __func__, __FILE__);
   Parsed_Cmd * parsed_cmd = calloc(1, sizeof(Parsed_Cmd));
   memcpy(parsed_cmd->marker, PARSED_CMD_MARKER, 4);
   // n. all flags are false, byte values 0, integers 0, pointers NULL because of calloc
   parsed_cmd->output_level = OL_DEFAULT;
   return parsed_cmd;
}


// Debugging function
void show_parsed_cmd(Parsed_Cmd * parsed_cmd) {
   int depth=0;
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_structure_loc("Parsed_Cmd", parsed_cmd, depth);
   rpt_int( "cmd_id",      NULL,  parsed_cmd->cmd_id,                 d1);
   // rptStructureLoc("dref", parsed_cmd->dref,                       d1);
   // if (parsed_cmd->dref)
   //    report_display_ref(parsed_cmd->dref, d2);
   rpt_structure_loc("pdid", parsed_cmd->pdid,                        d1);
   if (parsed_cmd->pdid)
       report_display_identifier(parsed_cmd->pdid,                    d2);
   rpt_bool("stats",       NULL,  parsed_cmd->stats,                  d1);
   rpt_bool("ddcdata",     NULL,  parsed_cmd->ddcdata,                d1);
#ifdef OLD
   rpt_str( "msg_level",   NULL,  msg_level_name(parsed_cmd->msg_level), d1);
   rpt_bool("prog output", NULL,  parsed_cmd->programmatic_output,    d1);
#endif
   rpt_str( "output_level",NULL,  output_level_name(parsed_cmd->output_level), d1);
   rpt_bool("force",       NULL,  parsed_cmd->force,                  d1);
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


