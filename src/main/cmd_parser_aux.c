/*
 * cmd_parser_base.c
 *
 *  Created on: Nov 24, 2015
 *      Author: rock
 */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "util/string_util.h"

#include "base/common.h"

#include "main/parsed_cmd.h"
#include "main/cmd_parser_aux.h"

//
// Command Description data structure
//


static Cmd_Desc cmdinfo[] = {
 // cmd_id              cmd_name  minchars min_arg_ct max_arg_ct
   {CMDID_DETECT,       "detect",        3,  0,       0},
// {CMDID_INFO,         "information",   3,  0,       0},
   {CMDID_CAPABILITIES, "capabilities",  3,  0,       0},
   {CMDID_GETVCP,       "getvcp",        3,  1,       1},
   {CMDID_SETVCP,       "setvcp",        3,  2,       MAX_SETVCP_VALUES*2},
   {CMDID_LISTVCP,      "listvcp",       5,  0,       0},
   {CMDID_TESTCASE,     "testcase",      3,  1,       1},
   {CMDID_LISTTESTS,    "listtests",     5,  0,       0},
   {CMDID_LOADVCP,      "loadvcp",       3,  1,       1},
   {CMDID_DUMPVCP,      "dumpvcp",       3,  0,       1},
};
static int cmdct = sizeof(cmdinfo)/sizeof(Cmd_Desc);


void validate_cmdinfo() {
   int ndx = 0;
   for (; ndx < cmdct; ndx++) {
      assert( cmdinfo[ndx].max_arg_ct <= MAX_ARGS);
   }
}


void show_cmd_desc(Cmd_Desc * cmd_desc) {
   printf("CmdDesc at %p\n", cmd_desc);
   printf("   cmd_id:     %d\n", cmd_desc->cmd_id);
   printf("   cmd_name:   %s\n", cmd_desc->cmd_name);
   printf("   minchars:   %d\n", cmd_desc->minchars);
   printf("   min_arg_ct: %d\n", cmd_desc->min_arg_ct);
   printf("   max_arg_ct: %d\n", cmd_desc->max_arg_ct);
}


Cmd_Desc * find_command(char * cmd) {
   Cmd_Desc * result = NULL;
   int ndx = 0;
   for (; ndx < cmdct; ndx++) {
      Cmd_Desc desc = cmdinfo[ndx];
      if (is_abbrev(cmd, desc.cmd_name, desc.minchars)) {
         result = &cmdinfo[ndx];
      }
   }
   // printf("(%s) cmd=|%s|, returning %p\n", __func__, cmd, result);
   return result;
}


Cmd_Desc * get_command(int cmdid) {
   bool debug = true;
   Cmd_Desc * result = NULL;
   int ndx = 0;
   for (; ndx < cmdct; ndx++) {
      Cmd_Desc desc = cmdinfo[ndx];
      if (cmdid == desc.cmd_id)  {
         result = &cmdinfo[ndx];
      }
   }
   if (debug) {
      printf("(%s) cmdid=|%d|, returning %p\n", __func__, cmdid, result);
      show_cmd_desc(result);
   }
   return result;
}


void init_cmd_parser_base() {
   validate_cmdinfo();
}



bool all_digits(char * val, int ct) {
   bool debug = false;
   if (debug)
      printf("(%s) ct-%d, val -> |%.*s|\n", __func__, ct, ct, val );
   bool ok = true;
   int ndx;
   for (ndx = 0; ndx < ct; ndx++) {
      if ( !isdigit(*(val+ndx)) ) {
         ok = false;
         break;
      }
   }
   if (debug)
      printf("(%s) Returning: %d  \n", __func__, ok );
   return ok;
}


bool parse_adl_arg(const char * val, int * piAdapterIndex, int * piDisplayIndex) {
   int rc = sscanf(val, "%d.%d", piAdapterIndex, piDisplayIndex);
   // printf("(%s) val=|%s| sscanf() returned %d  \n", __func__, val, rc );
   bool ok = (rc == 2);
   return ok;
}


bool parse_int_arg(char * val, int * pIval) {
   int ct = sscanf(val, "%d", pIval);
   return (ct == 1);
}

bool validate_output_level(Parsed_Cmd* parsed_cmd) {
   bool ok = true;
   // check that output_level consistent with cmd_id
   Byte valid_output_levels;
   Byte default_output_level = OL_NORMAL;
   switch(parsed_cmd->cmd_id) {
      case (CMDID_DETECT):
         valid_output_levels = OL_PROGRAM | OL_TERSE | OL_NORMAL | OL_VERBOSE;
         break;
      case (CMDID_GETVCP):
         valid_output_levels = OL_PROGRAM | OL_TERSE | OL_NORMAL | OL_VERBOSE;
         break;
      case (CMDID_DUMPVCP):
         valid_output_levels = OL_PROGRAM;
         default_output_level = OL_PROGRAM;
         break;
      default:
         default_output_level = OL_NORMAL;
         valid_output_levels = OL_TERSE | OL_NORMAL | OL_VERBOSE;
   }

   if (parsed_cmd->output_level == OL_DEFAULT)
         parsed_cmd->output_level = default_output_level;
   if (!(parsed_cmd->output_level & valid_output_levels)) {
      printf("Output level invalid for command %s: %s\n",
             get_command(parsed_cmd->cmd_id)->cmd_name,
             output_level_name(parsed_cmd->output_level) );
      ok = false;
   }
   return ok;
}

