/* cmd_parser_aux.c
 *
 * Created on: Nov 24, 2015
 *     Author: rock
 *
 * Functions and strings that are independent of the parser
 * package used.
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "util/string_util.h"

#include "base/common.h"

#include "cmdline/parsed_cmd.h"
#include "cmdline/cmd_parser_aux.h"

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
   {CMDID_INTERROGATE,  "interrogate",   3,  0,       0},
   {CMDID_ENVIRONMENT,  "environment",   3,  0,       0},
   {CMDID_VCPINFO,      "vcpinfo",       5,  0,       1},
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
   // DBGMSG("cmd=|%s|, returning %p", cmd, result);
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
      DBGMSG("cmdid=|%d|, returning %p", cmdid, result);
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
      DBGMSG("ct-%d, val -> |%.*s|", ct, ct, val );
   bool ok = true;
   int ndx;
   for (ndx = 0; ndx < ct; ndx++) {
      if ( !isdigit(*(val+ndx)) ) {
         ok = false;
         break;
      }
   }
   if (debug)
      DBGMSG("Returning: %d  ", ok );
   return ok;
}


bool parse_adl_arg(const char * val, int * piAdapterIndex, int * piDisplayIndex) {
   int rc = sscanf(val, "%d.%d", piAdapterIndex, piDisplayIndex);
   // DBGMSG("val=|%s| sscanf() returned %d  ", val, rc );
   bool ok = (rc == 2);
   return ok;
}


bool parse_int_arg(char * val, int * pIval) {
   int ct = sscanf(val, "%d", pIval);
   return (ct == 1);
}

typedef struct feature_subset_table_entry_s {
   Byte       subset_id;
   int        min_chars;
   char *     subset_name;
   char *     subset_desc;
} Feature_Subset_Table_Entry;

Feature_Subset_Table_Entry subset_table[] = {
      {SUBSET_ALL,        3,  "ALL",       "All known features"},
      {SUBSET_SUPPORTED,  3, "SUPPORTED",  "All known features that are valid for the display"},
      {SUBSET_SCAN,       3, "SCAN",       "All feature codes 00..FF, except those known to be WO"},
      {SUBSET_PROFILE,    3, "PROFILE",    "Features for color profile management"},
#ifdef FUTURE
      {SUBSET_COLOR,      3, "COLOR",      "Color related features"},
      {SUBSET_LUT,        3, "LUT",        "LUT related features"},
      {SUBSET_AUDIO,      3, "AUDIO",      "Audio related features"},
      {SUBSET_WINDOW,     3, "WINDOW",     "Window related features"},
      {SUBSET_TV,         2, "TV",         "TV related features"},
#endif
};






bool parse_feature_id_or_subset(char * val, Feature_Set_Ref * fsref) {
   bool ok = true;
   char * us = strdup( val );
   char * p = us;
   while (*p) {*p=toupper(*p); p++; }

   // TODO: replace with table
   if ( streq(us,"ALL" ))
      fsref->subset = SUBSET_ALL;
   else if ( is_abbrev(us,"SUPPORTED",3 ))
      fsref->subset = SUBSET_SUPPORTED;
   else if ( is_abbrev(us,"SCAN",3 ) )
      fsref->subset = SUBSET_SCAN;
   else if ( is_abbrev(us, "COLORMGT",3) )
      fsref->subset = SUBSET_COLORMGT;
   else if ( is_abbrev(us, "PROFILE",3) )
      fsref->subset = SUBSET_PROFILE;
   else {
     Byte feature_hexid = 0;   // temp
     ok = hhs_to_byte_in_buf(val, &feature_hexid);
     if (ok) {
        fsref->subset = SUBSET_SINGLE_FEATURE;
        fsref->specific_feature = feature_hexid;
     }
  }
  return ok;
}



bool validate_output_level(Parsed_Cmd* parsed_cmd) {
   // printf("(%s) parsed_cmd->cmdid = %d, parsed_cmd->output_level = %s\n",
   //        __func__, parsed_cmd->cmd_id,
   //        output_level_name(parsed_cmd->output_level));
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

   if (parsed_cmd->output_level == OL_DEFAULT) {
      parsed_cmd->output_level = default_output_level;
   }
   if (!(parsed_cmd->output_level & valid_output_levels)) {
      printf("Output level invalid for command %s: %s\n",
             get_command(parsed_cmd->cmd_id)->cmd_name,
             output_level_name(parsed_cmd->output_level) );
      ok = false;
   }
   return ok;
}


char * commands_list_help =
       "Commands:\n"
       "   detect\n"
       "   capabilities\n"
//     "   info\n"
//     "   listvcp\n"
       "   vcpinfo (feature-code-or-group)\n"
       "   getvcp <feature-code-or-group>\n"
       "   setvcp <feature-code> <new-value>\n"
       "   dumpvcp (filename)\n"
       "   loadvcp <filename>\n"
       "   testcase <testcase-number>\n"
       "   listtests\n"
       "   environment\n"
       "   interrogate\n"
       "\n";

char * command_argument_help =
       "Command Arguments\n"
       "  getvcp, vcpinfo:\n"
       "    <feature-code-or-group> can be any of the following:\n"
       "      - the hex feature code for a specific feature, with or without a leading 0x,\n"
       "        e.g. 10 or 0x10\n"
       "      - ALL       - all known feature codes\n"
       "      - COLORMGT  - color related feature codes\n"
       "      - PROFILE   - color related codes for profile management\n"
       "      - SUPPORTED - scan all known features codes, but only show supported codes\n"
       "      - SCAN      - scan all feature codes 0x00..0xff\n"
       "    Keywords can be abbreviated to the first 3 characters.\n"
       "    Case is ignored.  e.g. \"COL\", \"pro\"\n"
       "\n"
       "  setvcp:\n"
       "    <feature-code>: hexadecimal feature code, with or without a leading 0x,\n"
       "       e.g. 10 or 0x10\n"
       "    <new-value>: a decimal number in the range 0..255, or a single byte hex value,\n"
       "       e.g. 0x80\n"
      ;

char * monitor_selection_option_help =
       "Monitor Selection\n"
       "  The monitor to be processed can be specified using any of the options:\n"
       "  --display, --bus, --adl, --model and --sn, --edid\n"
       "  --display <display_number>, where <display_number> ranges from 1 to the number of\n"
       "    displays detected\n"
       "  --bus <bus number>, for /dev/i2c-<bus number>\n"
       "  --adl <adapter_number.display_number>, for monitors connected to an AMD video card\n"
       "          running AMD's proprietary video driver (ADL is an acronym for AMD Display Library)\n"
       "  --edid <hex string>, where <hex string> is a 256 hex character representation of the\n"
       "          128 byte first block of the EDID\n"
       "  --model <model_name>, where <model name> is as reported by the EDID\n"
       "  --sn <serial_number>, where <serial_number> is the string form of the serial number\n"
       "          reported by the EDID\n"
       "  Options --model and --sn must be specfied together.\n"
       "\n"
      ;

char * tracing_comma_separated_option_help =
       "Tracing:\n"
       "  The argument to --trace is a comma separated list of trace classes, surrounded by \n"
       "  quotation marks if necessary."
       "  e.g. --trace all, --trace \"I2C,ADL\"\n"
       "  Valid trace classes are:  BASE, I2C, ADL, DDC, TOP, ALL.\n"
       "  Trace class names are not case sensitive.\n"
       "  (Some trace classes are more useful than others.)\n"
  //   "\n"
      ;

char * tracing_multiple_call_option_help =
       "Tracing:\n"
       "  The argument to --trace is a trace class.  Specify the --trace option multiple\n"
       "  times to activate multiple trace classes, e.g. \"--trace i2c --trace ddc\"\n"
       "  Valid trace classes are:  BASE, I2C, ADL, DDC, TOP, ALL.\n"
       "  Trace class names are not case sensitive.\n"
       "  (Some trace classes are more useful than others.)\n"
  //   "\n"
      ;

char * stats_multiple_call_option_help =
       "Stats:\n"
       "  The argument to --stats is a statistics class.  Specify the --stats option multiple\n"
       "  times to activate multiple statistics classes, e.g. \"--stats calls --stats errors\"\n"
       "  Valid statistics classes are:  TRY, TRIES, ERRS, ERRORS, CALLS, ALL.\n"
       "  Statistics class names are not case sensitive and can abbreviated to 3 characters.\n"
       "  If no argument is specified, or ALL is specified, then all statistics classes are\n"
       "  output.\n"
      ;

char * retries_option_help =
      "Retries:\n"
      "  The argument to --retries is a comma separated list of 3 values:\n"
      "    maximum write-only exchange count\n"
      "    maximum write-read exchange count\n"
      "    maximum multi-part-read exchange count\n"
      "  A value of \"\" or \".\" leaves the default value unchanged\n"
      "  e.g. --retries \",.,15\" changes only the maximum multi-part-read exchange count"
      ;
