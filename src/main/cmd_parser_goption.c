/*
 * cmd_parser_goption.c
 *
 *  Created on: Nov 24, 2015
 *      Author: rock
 */



#include <assert.h>
#include <ctype.h>
#include <glib.h>
// #include <popt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <termios.h>
// #include <sys/ioctl.h>

#include "util/string_util.h"
#include "util/report_util.h"

#include "base/common.h"
#include "base/displays.h"
#include "base/msg_control.h"

#include "base/util.h"

#include "main/parsed_cmd.h"
#include "main/cmd_parser_aux.h"
#include "main/cmd_parser.h"



static char * commands_list =
                     "Commands:\n"
                     "   detect\n"
                     "   capabilities\n"
       //            "   info\n"
                     "   listvcp\n"
                     "   getvcp <feature-code>\n"
                     "   setvcp <feature-code> <new-value>\n"
                     "   dumpvcp (filename)\n"
                     "   loadvcp <filename>\n"
                     "   testcase <testcase-number>\n"
                     "   listtests\n"
                     "\n";

char * command_argument_help =
           "Command Arguments\n"
           "  getvcp:\n"
           "    <feature-code> can be any of the following:\n"
           "      - the hex feature code for a specific feature, with or without a leading 0x, e.g. 10 or 0x10\n"
           "      - ALL       - all known feature codes\n"
           "      - COLORMGT  - color related feature codes\n"
           "      - PROFILE   - color related codes for profile management\n"
           "      - SUPPORTED - scan all know features codes, but only show supported codes"
           "      - SCAN      - scan all feature codes 0x00..0xff\n"
           "    Keywords can be abbreviated to the first 3 characters.\n"
           "    Case is ignored.  e.g. \"COL\", \"pro\"\n"
           "\n"
           "  setvcp:\n"
           "    <feature-code>: hexadecimal feature code, with or without a leading 0x, e.g. 10 or 0x10\n"
           "    <new-value>: a decimal number in the range 0..255, or a single byte hex value, e.g. 0x80\n"
          ;

static char * monitor_selection_option_help =
      "Monitor Selection\n"
      "  The monitor to be processed can be specified using any of the options:\n"
      "  --display, --bus, --adl, --model and --sn, --edid\n"
      "  --display <display_number>, where <display_number> ranges from 1 to the number of displays detected\n"
      "  --bus <bus number>, for /dev/i2c-<bus number>\n"
      "  --adl <adapter_number.display_number>, for monitors connected to an AMD video card running\n"
      "          AMD's proprietary video driver (ADL is an acronym for AMD Display Library\n"
      "  --edid <hex string>, where <hex string> is a 256 hex character representation of the\n"
      "          128 byte first block of the EDID\n"
      "  --model <model_name>, where <model name> is as reported by the EDID\n"
      "  --sn <serial_number>, where <serial_number> is the string form of the serial number\n"
      "          reported by the EDID\n"
      "  Options --model and --sn must be specfied together.\n"
           "\n"
      ;

char * tracing_option_help =
           "Tracing:\n"
           "  The argument to --trace is a comma separated list of trace classes, surrounded by \n"
           "  quotation marks if necessary."
           "  e.g. --trace all, --trace \"I2C,ADL\"\n"
           "  Valid trace classes are:  BASE, I2C, ADL, DDC, TOP, ALL.\n"
           "  (Some trace classes are more useful than others.)\n"
        //   "\n"
           ;


static char * adlwork = NULL;
static Output_Level output_level = OL_NORMAL;
static int     iAdapterIndex = -1;
static int     iDisplayIndex = -1;

static gint    buswork   = -1;
static gint    dispwork  = -1;



gboolean adl_arg_func(const gchar* option_name, const gchar* value, gpointer data, GError** error) {
   bool debug = true;
   if (debug)
      printf("(%s) option_name=|%s|, value|%s|, data=%p\n", __func__, option_name, value, data);

   // int iAdapterIndex;
   // int iDisplayIndex;
   adlwork = strdup(value);   // alt way

   bool ok = parse_adl_arg(value, &iAdapterIndex, &iDisplayIndex);

   if (ok) {
      printf("(%s) parsed adl = %d.%d\n", __func__, iAdapterIndex, iDisplayIndex);
   }
   if (!ok) {
      // *error = G_OPTION_ERROR_FAILED;
      // alt?
      g_set_error(error, 0, G_OPTION_ERROR_FAILED, "bad adl" );
   }

   return ok;
}



gboolean output_arg_func(const gchar* option_name, const gchar* value, gpointer data, GError** error) {
   bool debug = true;
   if (debug)
      printf("(%s) option_name=|%s|, value|%s|, data=%p\n", __func__, option_name, value, data);

   if (streq(option_name, "-v") || streq(option_name, "--verbose") )
      output_level = OL_VERBOSE;
   else if (streq(option_name, "-t")  || streq(option_name, "--terse"))
      output_level = OL_TERSE;
   else if (streq(option_name, "-p") || streq(option_name, "--program"))
      output_level = OL_PROGRAM;
   else
      PROGRAM_LOGIC_ERROR("Unexpected option_name: %s", option_name);


   bool ok = true;
   if (!ok) {
      // *error = G_OPTION_ERROR_FAILED;
      // alt?
      g_set_error(error, 0, G_OPTION_ERROR_FAILED, "bad adl" );
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
 *    pointer to a ParsedCmd data structure if parsing successful
 *    NULL if execution should be terminated
 */
Parsed_Cmd * parse_command(int argc, char * argv[]) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting\n", __func__ );
   validate_cmdinfo();   // assertions

   if (debug) {
      printf("(%s) argc=%d\n", __func__, argc);
      int ndx = 0;
      for (; ndx < argc; ndx++) {
         printf("(%s) argv[%d] = |%s|\n", __func__, ndx, argv[ndx]);
      }
   }

   Parsed_Cmd * parsed_cmd = new_parsed_cmd();
   // parsed_cmd->pdid = create_busno_display_identifier(0);    // default monitor
   parsed_cmd->pdid = create_dispno_display_identifier(1);   // default monitor


   gboolean stats_flag   = false;
   gboolean ddc_flag     = false;
   gboolean force_flag   = false;
   gboolean version_flag = false;
   gboolean myhelp_flag  = false;
   gboolean myusage_flag = false;
   char * modelwork      = NULL;
   char * snwork         = NULL;
   char * edidwork       = NULL;
   char * tracework      = NULL;
   char** cmd_and_args   = NULL;
   char** trace_classes  = NULL;

   GOptionEntry option_entries[] = {
   //  long_name short flags option-type          gpointer             description                    arg description
      {"display", 'd',  0, G_OPTION_ARG_INT,      &dispwork,         "Display number",              "number"},
      {"bus",     'b',  0, G_OPTION_ARG_INT,      &buswork,          "I2C bus number",              "busNum" },
//    {"adl",     'a',  0, G_OPTION_ARG_CALLBACK, adl_arg_func,      "ADL adapter and display indexes", "adapterIndex.displayIndex"},
      {"adl",     'a',  0, G_OPTION_ARG_STRING,   &adlwork,          "ADL adapter and display indexes", "adapterIndex.displayIndex"},
      {"stats",   's',  0, G_OPTION_ARG_NONE,     &stats_flag,       "Show retry statistics", NULL},
      {"ddc",     '\0', 0, G_OPTION_ARG_NONE,     &ddc_flag,         "Report DDC protocol and data errors", NULL},
      {"verbose", 'v',  G_OPTION_FLAG_NO_ARG,
                           G_OPTION_ARG_CALLBACK, output_arg_func,   "Show extended detail",           NULL},
      {"terse",   't',  G_OPTION_FLAG_NO_ARG,
                           G_OPTION_ARG_CALLBACK, output_arg_func,   "Show brief detail",              NULL},
      {"program", 'p',  G_OPTION_FLAG_NO_ARG,
                           G_OPTION_ARG_CALLBACK, output_arg_func,   "Machine readable output",        NULL},
      {"force",   'f',  0, G_OPTION_ARG_NONE,     &force_flag,       "Do not check certain parms",     NULL},
      {"model",   'l',  0, G_OPTION_ARG_STRING,   &modelwork,        "Monitor model",                "model name"},
      {"sn",      'n',  0, G_OPTION_ARG_STRING,   &snwork,           "Monitor serial number",           "serial number"},
      {"edid",    'e',  0, G_OPTION_ARG_STRING,   &edidwork,         "Monitor EDID",  "256 char hex string" },
//    {"trace",   '\0', 0, G_OPTION_ARG_STRING_ARRAY, &trace_classes, "Trace classes",  "comma separated list" },
      {"trace",   '\0', 0, G_OPTION_ARG_STRING,   &tracework,        "Trace classes",  "comma separated list" },
      {"version", 'V',  0, G_OPTION_ARG_NONE,     &version_flag,     "Show version information", NULL},
      {"myusage", '\0', 0, G_OPTION_ARG_NONE,     &myusage_flag,     "Show usage", NULL},
      {"myhelp", '\0', 0,  G_OPTION_ARG_NONE,     &myhelp_flag,      "Show usage", NULL},
      {G_OPTION_REMAINING,
                 '\0', 0,  G_OPTION_ARG_STRING_ARRAY, &cmd_and_args, "ARGUMENTS description",   "command [arguments...]"},
      { NULL }
   };


   GError* error = NULL;
   GOptionContext* context;

   context = g_option_context_new("- DDC query and manipulation");
   g_option_context_add_main_entries(context, option_entries, NULL);
   // g_option_context_add_group(context,  gtk_get_option_group(TRUE));

   // comma delimited list of trace identifiers:
   // char * trace_group_string = strjoin(trace_group_names, trace_group_ct, ", ");
   // printf("(%s) traceGroupString = %s\n", __func__, traceGroupString);
   // const char * pieces[] = {tracing_option_help, "  Recognized trace classes: ", trace_group_string, "\n\n"};
   // tracing_option_help = strjoin(pieces, 4, NULL);

   // const char * pieces2[] = {command_argument_help, "  Recognized trace classes: ", trace_group_string, "\n\n"};
   // command_argument_help = strjoin(pieces, 4, NULL);

   const char * pieces3[] = {commands_list, command_argument_help};
   char * help_summary = strjoin(pieces3, 2, NULL);

   const char * pieces4[] = {monitor_selection_option_help    , tracing_option_help};
   char * help_description = strjoin(pieces4,2, NULL);

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

   // printf("(%s) buswork=%d\n", __func__, buswork);
   // printf("(%s) dispwork=%d\n", __func__, dispwork);
   // printf("(%s) stats_flag=%d\n", __func__, stats_flag);
   // printf("(%s) output_level=%d\n", __func__, output_level);
   if (trace_classes) {
      int ndx;
      for (; trace_classes[ndx] != NULL; ndx++)
         printf("(%s) trace class: %s\n", __func__, trace_classes[ndx]);
   }

   int explicit_display_spec_ct = 0;  // number of ways the display is explicitly specified

   if (adlwork) {
      // printf("(%s) case 'A', debug=%d\n", __func__, ok);
      if (debug)
         printf("(%s) case 'A', adlwork = |%s|\n", __func__, adlwork);
      int iAdapterIndex;
      int iDisplayIndex;
      bool adlok = parse_adl_arg(adlwork, &iAdapterIndex, &iDisplayIndex);
      if (!adlok) {
          printf("Invalid ADL argument: %s\n", adlwork );
          ok = false;
          // printf("(%s) After ADL parse, ok=%d\n", __func__, ok);
      }
      else {
         // parsedCmd->dref = createAdlDisplayRef(iAdapterIndex, iDisplayIndex);
         free(parsed_cmd->pdid);
         parsed_cmd->pdid = create_adlno_display_identifier(iAdapterIndex, iDisplayIndex);  // new way
      }
      explicit_display_spec_ct++;
   }

   if (buswork >= 0) {
      // printf("(%s) case B\n", __func__);
      free(parsed_cmd->pdid);
      parsed_cmd->pdid = create_busno_display_identifier(buswork);
      explicit_display_spec_ct++;
   }

   parsed_cmd->ddcdata = ddc_flag;

   if (dispwork >= 0) {
      // need to handle 0?
      // printf("(%s) case B\n", __func__);
      free(parsed_cmd->pdid);
      parsed_cmd->pdid = create_dispno_display_identifier(dispwork);
      explicit_display_spec_ct++;
   }

   if (edidwork) {
      if (strlen(edidwork) != 256) {
         printf("(%s) EDID hex string not 256 characters\n", __func__ );
         ok = false;
      }
      else {
         Byte * pba = NULL;
         int bytect = hhs_to_byte_array(edidwork, &pba);
         if (bytect < 0 || bytect != 128) {
            printf("(%s) Invalid EDID hex string\n", __func__ );
            ok = false;
         }
         else {
            free(parsed_cmd->pdid);
            parsed_cmd->pdid = create_edid_display_identifier(pba);  // new way
         }
         if (pba)
            free(pba);
      }
      explicit_display_spec_ct++;
   }

   parsed_cmd->force = force_flag;


   if (modelwork && snwork) {
      free(parsed_cmd->pdid);
      parsed_cmd->pdid = create_mon_ser_display_identifier(modelwork, snwork);  // new way
      explicit_display_spec_ct++;
   }
   else if (modelwork || snwork) {
      puts("--model and --sn must be specified together");
      ok = false;
   }

   parsed_cmd->output_level = output_level;
   parsed_cmd->stats = stats_flag;

   if (tracework) {
      if (debug)
         printf("(%s) case 'R', argument = |%s|\n", __func__, tracework );
      strupper(tracework);

      Trace_Group traceClasses = 0x00;
      if (strcmp(tracework, "ALL") == 0 ||
          strcmp(tracework, "*")   == 0) {
         // printf("(%s) Process ALL\n", __func__);
         traceClasses = 0xFF;
      }
      else {
         char * rest = tracework;
         char * token;
         char delim = ',';
         // originally token assignment was in while() clause, but valgrind
         // complaining about uninitialized variable, trying to figure out why
         token = strsep(&rest, &delim);
         while (token) {
            // printf("(%s) token: |%s|\n", __func__, token);
            Trace_Group tg = trace_class_name_to_value(token);
            // printf("(%s) tg=0x%02x\n", __func__, tg);
            if (tg) {
               traceClasses |= tg;
            }
            else {
               printf("Invalid trace group: %s\n", token);
               ok = false;
            }
            token = strsep(&rest, &delim);
         }
      }
         // printf("(%s) traceClasses = 0x%02x\n", __func__, traceClasses);
         parsed_cmd->trace = traceClasses;
      }

      if (version_flag) {
         printf("Compiled %s at %s\n", __DATE__, __TIME__ );
         exit(0);
      }

      if (myhelp_flag)
      {
         // printf("(%s) Customize help option implemented here\n", __func__);
         fprintf(stdout, "Usage: ddctool [options] command [command arguments]\n");
         fprintf(stdout, "%s", commands_list);
         fprintf(stdout, "%s", command_argument_help);
         // printf("(%s) Output of poptPrintHelp():\n", __func__);
         printf("Options:\n");
         // problem: poptPrintHelp begins with "ddctool [OPTIONS]:" line
         // poptPrintOptions  - my added function
        //  poptPrintOptions(pc, /*FILE * fp*/ stdout, /*@unused@*/ /* int flags */ 0);

         exit(0);

      }
      if (myusage_flag)
      {
         printf("(%s) Output of poptPrintUsage():\n", __func__);
         // poptPrintUsage(pc, /*FILE * fp*/ stdout, /*@unused@*/ /* int flags */ 0);
         fprintf(stdout, "        command [command-arguments]\n");
         fprintf(stdout, "%s", commands_list);
         exit(0);

      }

      if (explicit_display_spec_ct > 1) {
          puts("Display has been specified in more than 1 way");
          ok = false;
      }


   int rest_ct = 0;

   if (cmd_and_args) {
      for (; cmd_and_args[rest_ct] != NULL; rest_ct++) {
         if (debug) {
            printf("(%s) rest_ct=%d\n", __func__, rest_ct);
            printf("(%s) cmd_and_args: %s\n", __func__, cmd_and_args[rest_ct]);
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
               puts("Too many arguments");
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
            puts("Missing argument(s)");
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
       printf("(%s) Returning: %p\n", __func__, parsed_cmd);
   return parsed_cmd;
}
