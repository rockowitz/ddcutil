/* cmd_parser_goption.c
 *
 * Created on: Nov 24, 2015
 *     Author: rock
 *
 * Parse the command line using the glib goption functions.
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
#include <config.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/string_util.h"
#include "util/report_util.h"

#include "base/common.h"
#include "base/displays.h"
#include "base/msg_control.h"
#include "base/parms.h"
#include "base/util.h"

#include "cmdline/parsed_cmd.h"
#include "cmdline/cmd_parser_aux.h"
#include "cmdline/cmd_parser.h"

// Variables used by callback functions
static char * adlwork = NULL;
static Output_Level output_level = OL_DEFAULT;
static int     iAdapterIndex = -1;
static int     iDisplayIndex = -1;
static Stats_Type stats_work = STATS_NONE;

// not currently used
// Callback function for processing an --adl argument
gboolean adl_arg_func(const gchar* option_name,
                      const gchar* value,
                      gpointer     data,
                      GError**     error)
{
   bool debug = true;
   DBGMSF(debug, "option_name=|%s|, value|%s|, data=%p", option_name, value, data);

   // int iAdapterIndex;
   // int iDisplayIndex;
   adlwork = strdup(value);   // alt way

   bool ok = parse_adl_arg(value, &iAdapterIndex, &iDisplayIndex);

   if (ok) {
      DBGMSG("parsed adl = %d.%d", iAdapterIndex, iDisplayIndex);
   }
   if (!ok) {
      // *error = G_OPTION_ERROR_FAILED;
      // alt?
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "bad adl" );
   }

   return ok;
}


// Callback function for processing --terse, --verbose, --program
gboolean output_arg_func(const gchar* option_name,
                         const gchar* value,
                         gpointer     data,
                         GError**     error)
{
   bool debug = false;
   DBGMSF(debug, "option_name=|%s|, value|%s|, data=%p", option_name, value, data);

   if (streq(option_name, "-v") || streq(option_name, "--verbose") )
      output_level = OL_VERBOSE;
   else if (streq(option_name, "-t")  || streq(option_name, "--terse"))
      output_level = OL_TERSE;
   else if (streq(option_name, "-p") || streq(option_name, "--program"))
      output_level = OL_PROGRAM;
   else
      PROGRAM_LOGIC_ERROR("Unexpected option_name: %s", option_name);

   return true;
}

// Callback function for processing --stats
gboolean stats_arg_func(const    gchar* option_name,
                        const    gchar* value,
                        gpointer data,
                        GError** error)
{
   bool debug = false;
   DBGMSF(debug,"option_name=|%s|, value|%s|, data=%p", option_name, value, data);
   bool ok = true;

   if (value) {
      char * v2 = strupper(strdup(value));
      if ( streq(v2,"ALL") ) {
         stats_work |= STATS_ALL;
      }
      else if (streq(v2,"TRY") || is_abbrev(v2, "TRIES",3)) {
         stats_work |= STATS_TRIES;
      }
      else if ( is_abbrev(v2, "CALLS",3)) {
         stats_work |= STATS_CALLS;
      }
      else if (streq(v2,"ERRS") || is_abbrev(v2, "ERRORS",3)) {
         stats_work |= STATS_ERRORS;
      }
      else
         ok = false;
   }
   else {
      stats_work = STATS_ALL;
   }

   if (!ok) {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "invalid stats type: %s", value );
   }
   return ok;
}

/* Primary parsing function
 *
 * Arguments:
 *    argc      number of command line arguments
 *    argv      array of pointers to command line arguments
 *
 * Returns:
 *    pointer to a Parsed_Cmd struct if parsing successful
 *    NULL if execution should be terminated
 */
Parsed_Cmd * parse_command(int argc, char * argv[]) {
   bool debug = false;
   if (debug)
      DBGMSG("Starting" );
   validate_cmdinfo();   // assertions

   if (debug) {
      DBGMSG("argc=%d", argc);
      int ndx = 0;
      for (; ndx < argc; ndx++) {
         DBGMSG("argv[%d] = |%s|", ndx, argv[ndx]);
      }
   }

   Parsed_Cmd * parsed_cmd = new_parsed_cmd();
   // parsed_cmd->pdid = create_dispno_display_identifier(1);   // default monitor
   // DBGMSG("After new_parsed_cmd(), parsed_cmd->output_level_name = %s", output_level_name(parsed_cmd->output_level));

   gboolean stats_flag     = false;
   gboolean ddc_flag       = false;
   gboolean force_flag     = false;
   gboolean version_flag   = false;
// gboolean myhelp_flag    = false;PARSED_CMD_MARKERoutput
// gboolean myusage_flag   = false;
   char *   modelwork      = NULL;
   char *   snwork         = NULL;
   char *   edidwork       = NULL;
// char *   tracework      = NULL;
   char**   cmd_and_args   = NULL;
   gchar**   trace_classes  = NULL;
   gint     buswork        = -1;
   gint     dispwork       = -1;
   char *   maxtrywork      = NULL;
   gint     sleep_strategy_work = -1;

   GOptionEntry option_entries[] = {
   //  long_name short flags option-type          gpointer           description                    arg description
      {"display", 'd',  0, G_OPTION_ARG_INT,      &dispwork,         "Display number",              "number"},
      {"bus",     'b',  0, G_OPTION_ARG_INT,      &buswork,          "I2C bus number",              "busNum" },
//    {"adl",     'a',  0, G_OPTION_ARG_CALLBACK, adl_arg_func,      "ADL adapter and display indexes", "adapterIndex.displayIndex"},
      {"adl",     'a',  0, G_OPTION_ARG_STRING,   &adlwork,          "ADL adapter and display indexes", "adapterIndex.displayIndex"},
      {"stats",   's',  G_OPTION_FLAG_OPTIONAL_ARG,
                           G_OPTION_ARG_CALLBACK, stats_arg_func,    "Show retry statistics",    "stats type"},
      {"ddc",     '\0', 0, G_OPTION_ARG_NONE,     &ddc_flag,         "Report DDC protocol and data errors", NULL},
      {"verbose", 'v',  G_OPTION_FLAG_NO_ARG,
                           G_OPTION_ARG_CALLBACK, output_arg_func,   "Show extended detail",           NULL},
      {"terse",   't',  G_OPTION_FLAG_NO_ARG,
                           G_OPTION_ARG_CALLBACK, output_arg_func,   "Show brief detail",              NULL},
      {"program", 'p',  G_OPTION_FLAG_NO_ARG,
                           G_OPTION_ARG_CALLBACK, output_arg_func,   "Machine readable output",        NULL},
      {"force",   'f',  0, G_OPTION_ARG_NONE,     &force_flag,       "Do not check certain parms",     NULL},
      {"model",   'l',  0, G_OPTION_ARG_STRING,   &modelwork,        "Monitor model",                     "model name"},
      {"sn",      'n',  0, G_OPTION_ARG_STRING,   &snwork,           "Monitor serial number",          "serial number"},
      {"edid",    'e',  0, G_OPTION_ARG_STRING,   &edidwork,         "Monitor EDID",            "256 char hex string" },
      {"trace",   '\0', 0, G_OPTION_ARG_STRING_ARRAY, &trace_classes, "Trace classes",         "comma separated list" },
//    {"trace",   '\0', 0, G_OPTION_ARG_STRING,   &tracework,        "Trace classes",          "comma separated list" },
      {"maxtries",'\0', 0, G_OPTION_ARG_STRING,   &maxtrywork,       "Max try adjustment",  "comma separated list" },
      {"version", 'V',  0, G_OPTION_ARG_NONE,     &version_flag,     "Show version information", NULL},
//    {"myusage", '\0', 0, G_OPTION_ARG_NONE,     &myusage_flag,     "Show usage", NULL},
//    {"myhelp", '\0', 0,  G_OPTION_ARG_NONE,     &myhelp_flag,      "Show usage", NULL},
      {"sleep-strategy",
                  'y', 0,  G_OPTION_ARG_INT,      &sleep_strategy_work, "Set sleep strategy", "strategy number" },
      {G_OPTION_REMAINING,
                 '\0', 0,  G_OPTION_ARG_STRING_ARRAY, &cmd_and_args, "ARGUMENTS description",   "command [arguments...]"},
      { NULL }
   };

   GError* error = NULL;
   GOptionContext* context = g_option_context_new("- DDC query and manipulation");
   g_option_context_add_main_entries(context, option_entries, NULL);
   // g_option_context_add_group(context,  gtk_get_option_group(TRUE));

   // comma delimited list of trace identifiers:
   // char * trace_group_string = strjoin(trace_group_names, trace_group_ct, ", ");
   // DBGMSG("traceGroupString = %s", traceGroupString);
   // const char * pieces[] = {tracing_option_help, "  Recognized trace classes: ", trace_group_string, "\n\n"};
   // tracing_option_help = strjoin(pieces, 4, NULL);

   // const char * pieces2[] = {command_argument_help, "  Recognized trace classes: ", trace_group_string, "\n\n"};
   // command_argument_help = strjoin(pieces, 4, NULL);

   const char * pieces3[] = {commands_list_help, command_argument_help};
   char * help_summary = strjoin(pieces3, 2, NULL);

   const char * pieces4[] = {monitor_selection_option_help,
                             tracing_multiple_call_option_help,
                             "\n",
                             stats_multiple_call_option_help,
                             "\n",
                             retries_option_help};
   char * help_description = strjoin(pieces4, 6, NULL);

   // on --help, comes after usage line, before option detail
   g_option_context_set_summary(context, help_summary);

   // on --help, comes at end after option detail
   g_option_context_set_description(context, help_description);

   g_option_context_set_help_enabled(context, true);
   // bool ok = false;

   bool ok = g_option_context_parse(context, &argc, &argv, &error);
   if (!ok) {
      fprintf(stderr, "Option parsing failed: %s\n", error->message);
   }

   // DBGMSG("buswork=%d", buswork);
   // DBGMSG("dispwork=%d", dispwork);
   // DBGMSG("stats_flag=%d", stats_flag);
   // DBGMSG("output_level=%d", output_level);
   // DBGMSG("stats3=0x%02x",stats_work);

   int explicit_display_spec_ct = 0;  // number of ways the display is explicitly specified

   parsed_cmd->ddcdata      = ddc_flag;
   parsed_cmd->force        = force_flag;
   parsed_cmd->output_level = output_level;
   parsed_cmd->stats_types       = stats_work;
   parsed_cmd->sleep_strategy = sleep_strategy_work;

   if (adlwork) {
#ifdef HAVE_ADL
      if (debug)
         DBGMSG("adlwork = |%s|", adlwork);
      int iAdapterIndex;
      int iDisplayIndex;
      bool adlok = parse_adl_arg(adlwork, &iAdapterIndex, &iDisplayIndex);
      if (!adlok) {
          printf("Invalid ADL argument: %s\n", adlwork );
          ok = false;
          // DBGMSG("After ADL parse, ok=%d", ok);
      }
      else {
         // parsedCmd->dref = createAdlDisplayRef(iAdapterIndex, iDisplayIndex);
         // free(parsed_cmd->pdid);
         parsed_cmd->pdid = create_adlno_display_identifier(iAdapterIndex, iDisplayIndex);  // new way
      }
      explicit_display_spec_ct++;
#else
      fprintf(stderr, "ddctool not built with support for AMD proprietary driver.  --adl option invalid.\n");
#endif
   }

   if (buswork >= 0) {
      // DBGMSG("case B");
      free(parsed_cmd->pdid);
      parsed_cmd->pdid = create_busno_display_identifier(buswork);
      explicit_display_spec_ct++;
   }

   if (dispwork >= 0) {
      // need to handle 0?
      // DBGMSG("case B");
      // free(parsed_cmd->pdid);
      parsed_cmd->pdid = create_dispno_display_identifier(dispwork);
      explicit_display_spec_ct++;
   }

   if (edidwork) {
      if (strlen(edidwork) != 256) {
         fprintf(stderr, "EDID hex string not 256 characters\n");
         ok = false;
      }
      else {
         Byte * pba = NULL;
         int bytect = hhs_to_byte_array(edidwork, &pba);
         if (bytect < 0 || bytect != 128) {
            fprintf(stderr, "Invalid EDID hex string\n");
            ok = false;
         }
         else {
            // free(parsed_cmd->pdid);
            parsed_cmd->pdid = create_edid_display_identifier(pba);  // new way
         }
         if (pba)
            free(pba);
      }
      explicit_display_spec_ct++;
   }

   if (modelwork && snwork) {
      // free(parsed_cmd->pdid);
      parsed_cmd->pdid = create_mon_ser_display_identifier(modelwork, snwork);  // new way
      explicit_display_spec_ct++;
   }
   else if (modelwork || snwork) {
      fprintf(stderr, "--model and --sn must be specified together\n");
      ok = false;
   }

   if (maxtrywork) {
       bool saved_debug = debug;
       debug = false;
       if (debug)
          DBGMSG("retrywork, argument = |%s|", maxtrywork );

       Null_Terminated_String_Array pieces = strsplit(maxtrywork, ",");
       int ntsal = null_terminated_string_array_length(pieces);
       if (debug)
          DBGMSG("ntsal=%d", ntsal );
       if (null_terminated_string_array_length(pieces) != 3) {
          fprintf(stderr, "--retries requires 3 values\n");
          ok = false;
       }
       else {
          int ndx = 0;
          char trimmed_piece[10];
          for (; pieces[ndx] != NULL; ndx++) {
             char * token = strtrim_r(pieces[ndx], trimmed_piece, 10);
             // DBGMSG("token=|%s|", token);
             if (strlen(token) > 0 && !streq(token,".")) {
                int ival;
                int ct = sscanf(token, "%d", &ival);
                if (ct != 1) {
                   fprintf(stderr, "Invalid --maxtries value: %s\n", token);
                   ok = false;
                }
                else if (ival > MAX_MAX_TRIES) {
                   fprintf(stderr, "--maxtries value %d exceeds %d\n", ival, MAX_MAX_TRIES);
                   ok = false;
                }
                else {
                   parsed_cmd->max_tries[ndx] = ival;
                }
             }
          }
          assert(ndx == ntsal);
       }
       null_terminated_string_array_free(pieces);

       if (debug)
          DBGMSG("retries = %d,%d,%d", parsed_cmd->max_tries[0], parsed_cmd->max_tries[1], parsed_cmd->max_tries[2]);
       debug = saved_debug;
    }



#ifdef COMMA_DELIMITED_TRACE
   if (tracework) {
       bool saved_debug = debug;
       debug = true;
       if (debug)
          DBGMSG("tracework, argument = |%s|", tracework );
       strupper(tracework);
       Trace_Group traceClasses = 0x00;

       Null_Terminated_String_Array pieces = strsplit(tracework, ',');
       int ndx = 0;
       for (; pieces[ndx] != NULL; ndx++) {
          char * token = pieces[ndx];
          // TODO: deal with leading or trailing whitespace
          DBGMSG("token=|%s|", token);
          if (streq(token, "ALL") || streq(token, "*"))
             traceClasses = 0xff;
          else {
             // DBGMSG("token: |%s|", token);
             Trace_Group tg = trace_class_name_to_value(token);
             // DBGMSG("tg=0x%02x", tg);
             if (tg) {
                traceClasses |= tg;
             }
             else {
                printf("Invalid trace group: %s\n", token);
                ok = false;
             }
          }
       }
       DBGMSG("ndx=%d", ndx);
       DBGMSG("ntsal=%d", null_terminated_string_array_length(pieces) );
       assert(ndx == null_terminated_string_array_length(pieces));
       null_terminated_string_array_free(pieces);

       DBGMSG("traceClasses = 0x%02x", traceClasses);
       parsed_cmd->trace = traceClasses;
       debug = saved_debug;
    }
#endif

// #ifdef MULTIPLE_TRACE
   if (trace_classes) {
      Trace_Group traceClasses = 0x00;
      int ndx = 0;
      for (;trace_classes[ndx] != NULL; ndx++) {
         char * token = trace_classes[ndx];
         strupper(token);
         // DBGMSG("token=|%s|", token);
         if (streq(token, "ALL") || streq(token, "*"))
            traceClasses = 0xff;
         else {
            // DBGMSG("token: |%s|", token);
            Trace_Group tg = trace_class_name_to_value(token);
            // DBGMSG("tg=0x%02x", tg);
            if (tg) {
               traceClasses |= tg;
            }
            else {
               printf("Invalid trace group: %s\n", token);
               ok = false;
            }
        }
      }
      // DBGMSG("traceClasses = 0x%02x", traceClasses);
      parsed_cmd->trace = traceClasses;
   }
// #endif

   if (version_flag) {
      printf("Compiled %s at %s\n", __DATE__, __TIME__ );
#ifdef HAVE_ADL
      printf("Built with support for AMD Display Library (AMD proprietary driver)\n");
#else
      printf("Built without support for AMD Display Library (AMD proprietary driver)\n");
#endif
      exit(0);
   }

   // All options processed.  Check for consistency, set defaults
   if (explicit_display_spec_ct > 1) {
      fprintf(stderr, "Monitor specified in more than one way\n");
      ok = false;
   }
   else if (explicit_display_spec_ct == 0)
      parsed_cmd->pdid = create_dispno_display_identifier(1);   // default monitor


#ifdef NO
   if (myhelp_flag)
   {
      // DBGMSG("Customize help option implemented here");
      fprintf(stdout, "Usage: ddctool [options] command [command arguments]\n");
      fprintf(stdout, "%s", commands_list_help);
      fprintf(stdout, "%s", command_argument_help);
      // DBGMSG("Output of poptPrintHelp():");
      printf("Options:\n");
      // problem: poptPrintHelp begins with "ddctool [OPTIONS]:" line
      // poptPrintOptions  - my added function
     //  poptPrintOptions(pc, /*FILE * fp*/ stdout, /*@unused@*/ /* int flags */ 0);
      exit(0);

   }
   if (myusage_flag)
   {
      DBGMSG("Output of poptPrintUsage():");
      // poptPrintUsage(pc, /*FILE * fp*/ stdout, /*@unused@*/ /* int flags */ 0);
      fprintf(stdout, "        command [command-arguments]\n");
      fprintf(stdout, "%s", commands_list_help);
      exit(0);
   }

   if (explicit_display_spec_ct > 1) {
       puts("Display has been specified in more than 1 way");
       ok = false;
   }
#endif

   int rest_ct = 0;
   if (cmd_and_args) {
      for (; cmd_and_args[rest_ct] != NULL; rest_ct++) {
         if (debug) {
            DBGMSG("rest_ct=%d", rest_ct);
            DBGMSG("cmd_and_args: %s", cmd_and_args[rest_ct]);
         }
      }
   }

   if (rest_ct == 0) {
      fprintf(stderr, "No command specified\n");
      ok = false;
   }
   else {

      char * cmd = cmd_and_args[0];;
      if (debug)
         printf("cmd=|%s|\n", cmd);
      Cmd_Desc * cmdInfo = find_command(cmd);
      if (cmdInfo == NULL) {
         fprintf(stderr, "Unrecognized command: %s\n", cmd);
         ok = false;
      }
      else {
         if (debug)
            show_cmd_desc(cmdInfo);
         // process command args
         parsed_cmd->cmd_id  = cmdInfo->cmd_id;
         // parsedCmd->argCt  = cmdInfo->argct;
         int min_arg_ct = cmdInfo->min_arg_ct;
         int max_arg_ct = cmdInfo->max_arg_ct;
         int argctr = 1;
         while ( cmd_and_args[argctr] != NULL) {
            // printf("loop.  argctr=%d\n", argctr);
            if (argctr > max_arg_ct) {
               fprintf(stderr, "Too many arguments\n");
               ok = false;
               break;
            }
            char * thisarg = (char *) cmd_and_args[argctr];
            // printf("thisarg |%s|\n", thisarg);
            char * argcopy = strdup(thisarg);
            parsed_cmd->args[argctr-1] = argcopy;
            argctr++;
         }
         parsed_cmd->argct = argctr-1;

         // no more arguments specified
         if (argctr <= min_arg_ct) {
            fprintf(stderr, "Missing argument(s)\n");
            ok = false;
         }
      }
   }

   if (ok)
      ok = validate_output_level(parsed_cmd);

   g_option_context_free(context);

   if (debug)
      show_parsed_cmd(parsed_cmd);

   if (!ok) {
      free_parsed_cmd(parsed_cmd);
      parsed_cmd = NULL;
   }

   if (debug)
       DBGMSG("Returning: %p", parsed_cmd);
   return parsed_cmd;
}
