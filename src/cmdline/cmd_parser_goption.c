/** @file cmd_parser_goption.c
 *
 *  Parse the command line using the glib goption functions.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config.h>

#include <assert.h>
#include <config.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util/string_util.h"
#include "util/report_util.h"

#include "base/build_info.h"
#include "base/core.h"
#include "base/displays.h"
#include "base/parms.h"

#include "cmdline/cmd_parser_aux.h"
#include "cmdline/cmd_parser.h"
#include "cmdline/parsed_cmd.h"


// Variables used by callback functions
static char *            adlwork       = NULL;
static char *            usbwork       = NULL;
static DDCA_Output_Level output_level  = DDCA_OL_NORMAL;
static int               iAdapterIndex = -1;
static int               iDisplayIndex = -1;
static DDCA_Stats_Type   stats_work    = DDCA_STATS_NONE;


// not currently used
// Callback function for processing an --adl argument
gboolean adl_arg_func(const gchar* option_name,
                      const gchar* value,
                      gpointer     data,
                      GError**     error)
{
   bool debug = false;
   DBGMSF(debug, "option_name=|%s|, value|%s|, data=%p", option_name, value, data);

   adlwork = strdup(value);   // alt way
   bool ok = parse_dot_separated_arg(value, &iAdapterIndex, &iDisplayIndex);
   if (ok) {
      DBGMSG("parsed adl = %d.%d", iAdapterIndex, iDisplayIndex);
   }
   if (!ok) {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "bad adl" );
   }

   return ok;
}


// Callback function for processing --terse, --verbose and synonyms
gboolean output_arg_func(const gchar* option_name,
                         const gchar* value,
                         gpointer     data,
                         GError**     error)
{
   bool debug = false;
   DBGMSF(debug, "option_name=|%s|, value|%s|, data=%p", option_name, value, data);
   bool ok = true;

   if (streq(option_name, "-v") || streq(option_name, "--verbose") )
      output_level = DDCA_OL_VERBOSE;
   else if (streq(option_name, "-t")  || streq(option_name, "--terse") || streq(option_name, "--brief") )
      output_level = DDCA_OL_TERSE;
   else {
      PROGRAM_LOGIC_ERROR("Unexpected option_name: %s", option_name);
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                  "PROGRAM LOGIC ERROR: Unexpected option_name: %s", option_name);
      ok = false;
   }

   return ok;
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
         stats_work |= DDCA_STATS_ALL;
      }
      else if (streq(v2,"TRY") || is_abbrev(v2, "TRIES",3)) {
         stats_work |= DDCA_STATS_TRIES;
      }
      else if ( is_abbrev(v2, "CALLS",3)) {
         stats_work |= DDCA_STATS_CALLS;
      }
      else if (streq(v2,"ERRS") || is_abbrev(v2, "ERRORS",3)) {
         stats_work |= DDCA_STATS_ERRORS;
      }
      else if ( is_abbrev(v2,"ELAPSED",3) || is_abbrev(v2, "TIME",3)) {
         stats_work |= DDCA_STATS_ELAPSED;
      }
      else
         ok = false;
      free(v2);
   }
   else {
      stats_work = DDCA_STATS_ALL;
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
   DBGMSF(debug, "Starting" );
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

   gchar * original_command = g_strjoinv(" ",argv);
   // DBGMSG("original command: %s", original_command);
   parsed_cmd->raw_command = strdup(original_command);

// gboolean stats_flag     = false;
   gboolean ddc_flag       = false;
   gboolean force_flag     = false;
   gboolean force_slave_flag = false;
   gboolean show_unsupported_flag = false;
   gboolean version_flag   = false;
   gboolean timestamp_trace_flag = false;
   gboolean thread_id_trace_flag = false;
   gboolean verify_flag    = false;
   gboolean noverify_flag  = false;
   gboolean nodetect_flag  = false;
   gboolean async_flag     = false;
   gboolean report_freed_excp_flag = false;
   gboolean notable_flag   = true;
   gboolean rw_only_flag   = false;
   gboolean ro_only_flag   = false;
   gboolean wo_only_flag   = false;
   gboolean enable_udf_flag = false;
#ifdef USE_USB
   gboolean enable_usb_flag = true;
#endif
   gboolean timeout_i2c_io_flag = false;
   gboolean reduce_sleeps_flag  = false;
   gboolean deferred_sleep_flag = false;
   gboolean dsa_flag       = false;
   gboolean f1_flag        = false;
   gboolean f2_flag        = false;
   gboolean f3_flag        = false;
   gboolean f4_flag        = false;
   gboolean f5_flag        = false;
   gboolean f6_flag        = false;
   gboolean debug_parse_flag = false;
   char *   mfg_id_work    = NULL;
   char *   modelwork      = NULL;
   char *   snwork         = NULL;
   char *   edidwork       = NULL;
   char *   mccswork       = NULL;   // MCCS version
// char *   tracework      = NULL;
   char**   cmd_and_args   = NULL;
   gchar**  trace_classes  = NULL;
   gchar**  trace_functions = NULL;
   gchar**  trace_filenames = NULL;
   gint     buswork        = -1;
   gint     hidwork        = -1;
   gint     dispwork       = -1;
   char *   maxtrywork      = NULL;
   gint     i1_work = -1;
   char *   failsim_fn_work = NULL;
   // gboolean enable_failsim_flag = false;
   char *   sleep_multiplier_work = NULL;

   GOptionEntry option_entries[] = {
   //  long_name short flags option-type          gpointer           description                    arg description
   //  monitor selection options
      {"display", 'd',  0, G_OPTION_ARG_INT,      &dispwork,         "Display number",              "number"},
      {"dis",    '\0',  0, G_OPTION_ARG_INT,      &dispwork,         "Display number",              "number"},
      {"bus",     'b',  0, G_OPTION_ARG_INT,      &buswork,          "I2C bus number",              "busnum" },
      {"hiddev", '\0',  0, G_OPTION_ARG_INT,      &hidwork,          "hiddev device number",        "number" },
#ifdef HAVE_ADL
//    {"adl",     'a',  0, G_OPTION_ARG_CALLBACK, adl_arg_func,      "ADL adapter and display indexes", "adapterIndex.displayIndex"},
      {"adl",     'a',  0, G_OPTION_ARG_STRING,   &adlwork,          "ADL adapter and display indexes", "adapterIndex.displayIndex"},
#endif
      {"usb",     'u',  0, G_OPTION_ARG_STRING,   &usbwork,          "USB bus and device numbers",  "busnum.devicenum"},
      {"mfg",     'g',  0, G_OPTION_ARG_STRING,   &mfg_id_work,      "Monitor manufacturer code",   "mfg_id"},
      {"model",   'l',  0, G_OPTION_ARG_STRING,   &modelwork,        "Monitor model",               "model name"},
      {"sn",      'n',  0, G_OPTION_ARG_STRING,   &snwork,           "Monitor serial number",       "serial number"},
      {"edid",    'e',  0, G_OPTION_ARG_STRING,   &edidwork,         "Monitor EDID",            "256 char hex string" },
#ifdef USE_USB
//    {"disable-usb", '\0', G_OPTION_FLAG_REVERSE,
//                             G_OPTION_ARG_NONE, &enable_usb_flag,  "Do not detect USB devices", NULL},
      {"nousb",   '\0', G_OPTION_FLAG_REVERSE,
                               G_OPTION_ARG_NONE, &enable_usb_flag,  "Do not detect USB devices", NULL},

#endif
      // output control
      {"ddc",     '\0', 0, G_OPTION_ARG_NONE,     &ddc_flag,         "Report DDC protocol and data errors", NULL},
      {"verbose", 'v',  G_OPTION_FLAG_NO_ARG,
                           G_OPTION_ARG_CALLBACK, output_arg_func,   "Show extended detail",             NULL},
      {"terse",   't',  G_OPTION_FLAG_NO_ARG,
                           G_OPTION_ARG_CALLBACK, output_arg_func,   "Show brief detail",                NULL},
      {"brief",   '\0', G_OPTION_FLAG_NO_ARG,
                           G_OPTION_ARG_CALLBACK, output_arg_func,   "Show brief detail",                NULL},

      // feature selection filters
      {"show-unsupported",
                  'U',  0, G_OPTION_ARG_NONE,     &show_unsupported_flag, "Report unsupported features", NULL},
      {"notable", '\0', G_OPTION_FLAG_HIDDEN,
                           G_OPTION_ARG_NONE,     &notable_flag,     "Exclude table type feature codes",  NULL},
      {"no-table",'\0', 0, G_OPTION_ARG_NONE,     &notable_flag,     "Exclude table type feature codes",  NULL},
      {"show-table",'\0',G_OPTION_FLAG_REVERSE,
                           G_OPTION_ARG_NONE,     &notable_flag,     "Report table type feature codes",  NULL},
      {"rw",      '\0', 0, G_OPTION_ARG_NONE,     &rw_only_flag,     "Include only RW features",         NULL},
      {"ro",      '\0', 0, G_OPTION_ARG_NONE,     &ro_only_flag,     "Include only RO features",         NULL},
      {"wo",      '\0', 0, G_OPTION_ARG_NONE,     &wo_only_flag,     "Include only WO features",         NULL},

      // tuning
      {"maxtries",'\0', 0, G_OPTION_ARG_STRING,   &maxtrywork,       "Max try adjustment",  "comma separated list" },
      {"stats",   's',  G_OPTION_FLAG_OPTIONAL_ARG,
                           G_OPTION_ARG_CALLBACK, stats_arg_func,    "Show performance statistics",  "stats type"},
      {"force-slave-address",
                  '\0', 0, G_OPTION_ARG_NONE,     &force_slave_flag, "Force I2C slave address",         NULL},
      {"force",   'f',  G_OPTION_FLAG_HIDDEN,
                           G_OPTION_ARG_NONE,     &force_flag,       "Ignore certain checks",           NULL},
      {"verify",  '\0', 0, G_OPTION_ARG_NONE,     &verify_flag,      "Read VCP value after setting it", NULL},
      {"noverify",'\0', 0, G_OPTION_ARG_NONE,     &noverify_flag,    "Do not read VCP value after setting it", NULL},
//    {"nodetect",'\0', 0, G_OPTION_ARG_NONE,     &nodetect_flag,    "Skip initial monitor detection",  NULL},
      {"async",   '\0', 0, G_OPTION_ARG_NONE,     &async_flag,       "Enable asynchronous display detection", NULL},

      {"udf",     '\0', 0, G_OPTION_ARG_NONE,     &enable_udf_flag,  "Enable user defined feature support", NULL},
      {"noudf",   '\0', G_OPTION_FLAG_REVERSE,
                           G_OPTION_ARG_NONE,     &enable_udf_flag,  "Disable user defined feature support", NULL},

      // performance and retry
      {"sleep-multiplier", '\0', 0,
                           G_OPTION_ARG_STRING,   &sleep_multiplier_work, "Multiplication factor for DDC sleeps", "number"},
      {"timeout-i2c-io",'\0', 0, G_OPTION_ARG_NONE, &timeout_i2c_io_flag, "Wrap I2C IO in timeout",  NULL},
//    {"no-timeout-ddc-io",'\0',G_OPTION_FLAG_REVERSE,
//                            G_OPTION_ARG_NONE,  &timeout_i2c_io_flag,   "Do not wrap DDC IO in timeout (default)",  NULL},
      {"less-sleep" ,'\0', 0, G_OPTION_ARG_NONE, &reduce_sleeps_flag, "Eliminate some sleeps",  NULL},
      {"sleep-less" ,'\0', 0, G_OPTION_ARG_NONE, &reduce_sleeps_flag, "Eliminate some sleeps",  NULL},
//    {"reduce-sleeps",'\0', 0, G_OPTION_ARG_NONE, &reduce_sleeps_flag, "Eliminate some sleeps",  NULL},
//    {"no-reduce-sleeps",'\0',G_OPTION_FLAG_REVERSE,
//                               G_OPTION_ARG_NONE,  &reduce_sleeps_flag, "Do not eliminate any sleeps (default)",  NULL},
      {"lazy-sleep", '\0', 0, G_OPTION_ARG_NONE, &deferred_sleep_flag, "Delay sleeps if possible",  NULL},
//    {"defer-sleeps",'\0', 0, G_OPTION_ARG_NONE, &deferred_sleep_flag, "Delay sleeps if possible",  NULL},
      {"dynamic-sleep-adjustment",'\0', 0, G_OPTION_ARG_NONE, &dsa_flag, "Enable dynamic sleep adjustment",  NULL},
      {"dsa",                     '\0', 0, G_OPTION_ARG_NONE, &dsa_flag, "Enable dynamic sleep adjustment",  NULL},


      // debugging
      {"excp",    '\0', 0, G_OPTION_ARG_NONE,     &report_freed_excp_flag,  "Report freed exceptions", NULL},
      {"trace",   '\0', 0, G_OPTION_ARG_STRING_ARRAY, &trace_classes, "Trace classes",         "trace class name" },
//    {"trace",   '\0', 0, G_OPTION_ARG_STRING,   &tracework,        "Trace classes",          "comma separated list" },
      {"trcfunc", '\0',0, G_OPTION_ARG_STRING_ARRAY, &trace_functions, "Trace functions",     "function name" },
      {"trcfile", '\0',0, G_OPTION_ARG_STRING_ARRAY, &trace_filenames,    "Trace files",     "file name" },

      {"timestamp",'\0',  0, G_OPTION_ARG_NONE,   &timestamp_trace_flag, "Prepend trace msgs with elapsed time",  NULL},
      {"ts",      '\0',   0, G_OPTION_ARG_NONE,   &timestamp_trace_flag, "Prepend trace msgs with elapsed time",  NULL},
      {"thread-id",'\0',  0, G_OPTION_ARG_NONE,   &thread_id_trace_flag, "Prepend trace msgs with thread id",  NULL},
      {"tid",     '\0',   0, G_OPTION_ARG_NONE,   &thread_id_trace_flag, "Prepend trace msgs with thread id",  NULL},

//    {"myusage", '\0', 0, G_OPTION_ARG_NONE,     &myusage_flag,     "Show usage", NULL},
//    {"myhelp",  '\0', 0,  G_OPTION_ARG_NONE,     &myhelp_flag,      "Show usage", NULL},
      {"i1",      '\0', 0,  G_OPTION_ARG_INT,      &i1_work, "special", "non-negative number" },
      {"f1",      '\0', 0,  G_OPTION_ARG_NONE,     &f1_flag,         "Special flag 1",    NULL},
      {"f2",      '\0', 0,  G_OPTION_ARG_NONE,     &f2_flag,         "Special flag 2",    NULL},
      {"f3",      '\0', 0,  G_OPTION_ARG_NONE,     &f3_flag,         "Special flag 3",    NULL},
      {"f4",      '\0', 0,  G_OPTION_ARG_NONE,     &f4_flag,         "Special flag 4",    NULL},
      {"f5",      '\0', 0,  G_OPTION_ARG_NONE,     &f5_flag,         "Special flag 5",    NULL},
      {"f6",      '\0', 0,  G_OPTION_ARG_NONE,     &f6_flag,         "Special flag 6",    NULL},
      {"debug-parse", '\0', 0,  G_OPTION_ARG_NONE,  &debug_parse_flag,"Report parsed command",    NULL},
      {"failsim", '\0', 0,  G_OPTION_ARG_FILENAME, &failsim_fn_work, "Enable simulation", "control file name"},

      // other
      {"version", 'V',  0, G_OPTION_ARG_NONE,     &version_flag,     "Show version information", NULL},
      {"mccs",    '\0', 0, G_OPTION_ARG_STRING,   &mccswork,         "MCCS version",            "major.minor" },

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

   // const char * pieces3[] = {commands_list_help, command_argument_help};
   // char * help_summary = strjoin(pieces3, 2, NULL);

   char * cmd_args_help = assemble_command_argument_help();
   // const char * pieces3[] = {commands_list_help, command_argument_help, cmd_args_help}; // TEMP commands_list_help
   const char * pieces3[] = {commands_list_help, cmd_args_help};
   char * help_summary = strjoin(pieces3, 2, NULL);
   free(cmd_args_help);

   const char * pieces4[] = {monitor_selection_option_help,
                             tracing_multiple_call_option_help,
                             "\n",
                             trcfunc_multiple_call_option_help,
                             "\n",
                             trcfile_multiple_call_option_help,
                             "\n",
                             stats_multiple_call_option_help,
                             "\n",
                             maxtries_option_help};
   char * help_description = strjoin(pieces4, 10, NULL);

   // on --help, comes after usage line, before option detail
   g_option_context_set_summary(context, help_summary);
   free(help_summary);

   // on --help, comes at end after option detail
   g_option_context_set_description(context, help_description);
   free(help_description);

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

   int rwo_flag_ct = 0;
   if (rw_only_flag)   rwo_flag_ct++;
   if (ro_only_flag)   rwo_flag_ct++;
   if (wo_only_flag)   rwo_flag_ct++;
   if (rwo_flag_ct > 1) {
      fprintf(stderr, "Options -rw-only, --ro-only, --wo-only are mutually exclusive\n");
      ok = false;
   }


#define SET_CMDFLAG(_bit, _flag) \
   do { \
      if (_flag) \
         parsed_cmd->flags |= _bit; \
   } while(0)

   parsed_cmd->output_level     = output_level;
   parsed_cmd->stats_types      = stats_work;
   parsed_cmd->i1               = i1_work;
   SET_CMDFLAG(CMD_FLAG_DDCDATA,           ddc_flag);
   SET_CMDFLAG(CMD_FLAG_FORCE_SLAVE_ADDR,  force_slave_flag);
   SET_CMDFLAG(CMD_FLAG_TIMESTAMP_TRACE,   timestamp_trace_flag);
   SET_CMDFLAG(CMD_FLAG_THREAD_ID_TRACE,   thread_id_trace_flag);
   SET_CMDFLAG(CMD_FLAG_VERIFY,            verify_flag || !noverify_flag);
   // if (verify_flag || !noverify_flag)
   //    parsed_cmd->flags |= CMD_FLAG_VERIFY;
   SET_CMDFLAG(CMD_FLAG_NODETECT,          nodetect_flag);
   SET_CMDFLAG(CMD_FLAG_ASYNC,             async_flag);
   SET_CMDFLAG(CMD_FLAG_REPORT_FREED_EXCP, report_freed_excp_flag);
   SET_CMDFLAG(CMD_FLAG_NOTABLE,           notable_flag);
   SET_CMDFLAG(CMD_FLAG_SHOW_UNSUPPORTED,  show_unsupported_flag);
   SET_CMDFLAG(CMD_FLAG_RW_ONLY,           rw_only_flag);
   SET_CMDFLAG(CMD_FLAG_RO_ONLY,           ro_only_flag);
   SET_CMDFLAG(CMD_FLAG_WO_ONLY,           wo_only_flag);
   SET_CMDFLAG(CMD_FLAG_FORCE,             force_flag);
   SET_CMDFLAG(CMD_FLAG_ENABLE_UDF,        enable_udf_flag);
#ifdef USE_USB
   SET_CMDFLAG(CMD_FLAG_ENABLE_USB,        enable_usb_flag);
#endif
   SET_CMDFLAG(CMD_FLAG_TIMEOUT_I2C_IO,    timeout_i2c_io_flag);
   SET_CMDFLAG(CMD_FLAG_REDUCE_SLEEPS,     reduce_sleeps_flag);
   SET_CMDFLAG(CMD_FLAG_DSA,               dsa_flag);
   SET_CMDFLAG(CMD_FLAG_DEFER_SLEEPS,      deferred_sleep_flag);
   SET_CMDFLAG(CMD_FLAG_F1,                f1_flag);
   SET_CMDFLAG(CMD_FLAG_F2,                f2_flag);
   SET_CMDFLAG(CMD_FLAG_F3,                f3_flag);
   SET_CMDFLAG(CMD_FLAG_F4,                f4_flag);
   SET_CMDFLAG(CMD_FLAG_F5,                f5_flag);
   SET_CMDFLAG(CMD_FLAG_F6,                f6_flag);

   if (failsim_fn_work) {
#ifdef ENABLE_FAILSIM
      // parsed_cmd->enable_failure_simulation = true;
      parsed_cmd->flags |= CMD_FLAG_ENABLE_FAILSIM;
      parsed_cmd->failsim_control_fn = failsim_fn_work;
#else
      fprintf(stderr, "ddcutil not built with failure simulation support.  --failsim option invalid.\n");
      ok = false;
#endif
   }

#undef SET_CMDFLAG


   // Create display identifier
   //
   // n. at this point parsed_cmd->pdid == NULL

   if (adlwork) {
#ifdef HAVE_ADL
      if (debug)
         DBGMSG("adlwork = |%s|", adlwork);
      int iAdapterIndex;
      int iDisplayIndex;
      bool adlok = parse_dot_separated_arg(adlwork, &iAdapterIndex, &iDisplayIndex);
      if (!adlok) {
          fprintf(stderr, "Invalid ADL argument: %s\n", adlwork );
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
      fprintf(stderr, "ddcutil not built with support for AMD proprietary driver.  --adl option invalid.\n");
      ok = false;
#endif
   }


   if (usbwork) {
#ifdef USE_USB
      DBGMSF(debug, "usbwork = |%s|", usbwork);
      int busnum;
      int devicenum;
      bool arg_ok = parse_dot_separated_arg(usbwork, &busnum, &devicenum);
      if (!arg_ok)
         arg_ok = parse_colon_separated_arg(usbwork, &busnum, &devicenum);
      if (!arg_ok) {
          fprintf(stderr, "Invalid USB argument: %s\n", usbwork );
          ok = false;
          // DBGMSG("After USB parse, ok=%d", ok);
      }
      else {
         // avoid memory leak in case parsed_cmd->pdid set in more than 1 way
         if (parsed_cmd->pdid) {
            free_display_identifier(parsed_cmd->pdid);
         }
         parsed_cmd->pdid = create_usb_display_identifier(busnum, devicenum);
      }
      explicit_display_spec_ct++;
#else
      fprintf(stderr, "ddcutil not built with support for USB connected monitors.  --usb option invalid.\n");
      ok = false;
#endif
   }

   if (buswork >= 0) {
      // avoid memory leak in case parsed_cmd->pdid set in more than 1 way
      if (parsed_cmd->pdid)
         free_display_identifier(parsed_cmd->pdid);
      parsed_cmd->pdid = create_busno_display_identifier(buswork);
      explicit_display_spec_ct++;
   }

   if (hidwork >= 0) {
      // avoid memory leak in case parsed_cmd->pdid set in more than 1 way
      if (parsed_cmd->pdid)
         free_display_identifier(parsed_cmd->pdid);
      parsed_cmd->pdid = create_usb_hiddev_display_identifier(hidwork);
      explicit_display_spec_ct++;
   }

   if (dispwork >= 0) {
      // avoid memory leak in case parsed_cmd->pdid set in more than 1 way
      if (parsed_cmd->pdid)
         free_display_identifier(parsed_cmd->pdid);
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
            // avoid memory leak in case parsed_cmd->pdid set in more than 1 way
            if (parsed_cmd->pdid)
               free_display_identifier(parsed_cmd->pdid);
            parsed_cmd->pdid = create_edid_display_identifier(pba);  // new way
         }
         if (pba)
            free(pba);
      }
      explicit_display_spec_ct++;
   }

   if (mfg_id_work || modelwork || snwork) {
      // avoid memory leak in case parsed_cmd->pdid set in more than 1 way
      if (parsed_cmd->pdid)
         free_display_identifier(parsed_cmd->pdid);
      parsed_cmd->pdid = create_mfg_model_sn_display_identifier(
                          mfg_id_work,
                          modelwork,
                          snwork);
      explicit_display_spec_ct++;
   }

   if (maxtrywork) {
       bool saved_debug = debug;
       debug = false;
       DBGMSF(debug, "retrywork, argument = |%s|", maxtrywork );

       Null_Terminated_String_Array pieces = strsplit(maxtrywork, ",");
       int ntsal = ntsa_length(pieces);
       DBGMSF(debug, "ntsal=%d", ntsal );
       if (ntsa_length(pieces) != 3) {
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
                int ct = sscanf(token, "%ud", &ival);
                if (ct != 1) {
                   fprintf(stderr, "Invalid --maxtries value: %s\n", token);
                   ok = false;
                }
                else if (ival > MAX_MAX_TRIES) {
                   fprintf(stderr, "--maxtries value %d exceeds %d\n", ival, MAX_MAX_TRIES);
                   ok = false;
                }
                else if (ival < 0) {
                   fprintf(stderr, "negative --maxtries value: %d\n", ival);
                   ok = false;
                }

                else {
                   parsed_cmd->max_tries[ndx] = ival;
                }
             }
             // split max multipart exchange into read and write
             // not here, do it in main
             // parsed_cmd->max_tries[3] = parsed_cmd->max_tries[2];
          }
          assert(ndx == ntsal);
       }
       ntsa_free(pieces, /* free_strings */ true);

       DBGMSF(debug, "retries = %d,%d,%d", parsed_cmd->max_tries[0],
                                           parsed_cmd->max_tries[1],
                                           parsed_cmd->max_tries[2]);
       debug = saved_debug;
    }

   if (mccswork) {
      DBGMSF(debug, "mccswork = |%s|", mccswork);
      bool arg_ok = false;
      DDCA_MCCS_Version_Spec vspec = parse_vspec(mccswork);
      if (!vcp_version_eq(vspec, DDCA_VSPEC_UNKNOWN)) {
         arg_ok = vcp_version_is_valid(vspec, false);
      }
      if (!arg_ok) {
          fprintf(stderr, "Invalid MCCS spec: %s\n", mccswork );
          ok = false;
      }
      else {
         // which?
         parsed_cmd->mccs_vspec = vspec;
         parsed_cmd->mccs_version_id = mccs_version_spec_to_id(vspec);
      }
   }

   if (sleep_multiplier_work) {
      DBGMSF(debug, "sleep_multiplier_work = |%s|", sleep_multiplier_work);
      float multiplier = 0.0f;
      bool arg_ok = str_to_float(sleep_multiplier_work, &multiplier);
      if (arg_ok) {
         if (multiplier <= 0.0f || multiplier >= 100.0)
            arg_ok = false;
      }

      if (!arg_ok) {
          fprintf(stderr, "Invalid sleep-multiplier: %s\n", sleep_multiplier_work );
          ok = false;
      }
      else {
         parsed_cmd->sleep_multiplier = multiplier;
      }
   }


#ifdef COMMA_DELIMITED_TRACE
   if (tracework) {
       bool saved_debug = debug;
       debug = false;
       if (debug)
          DBGMSG("tracework, argument = |%s|", tracework );
       strupper(tracework);
       DDCA_Trace_Group traceClasses = 0x00;

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
             DDCA_Trace_Group tg = trace_class_name_to_value(token);
             // DBGMSG("tg=0x%02x", tg);
             if (tg) {
                traceClasses |= tg;
             }
             else {
                fprintf(stderr, "Invalid trace group: %s\n", token);
                ok = false;
             }
          }
       }
       DBGMSG("ndx=%d", ndx);
       DBGMSG("ntsal=%d", ntsa_length(pieces) );
       assert(ndx == ntsa_length(pieces));
       ntsa_free(pieces);

       DBGMSG("traceClasses = 0x%02x", traceClasses);
       parsed_cmd->traced_groups = traceClasses;
       debug = saved_debug;
    }
#endif

// #ifdef MULTIPLE_TRACE
   if (trace_classes) {
      DDCA_Trace_Group traceClasses = 0x00;
      int ndx = 0;
      for (;trace_classes[ndx] != NULL; ndx++) {
         char * token = trace_classes[ndx];
         strupper(token);
         // DBGMSG("token=|%s|", token);
         if (streq(token, "ALL") || streq(token, "*")) {
            traceClasses = DDCA_TRC_ALL;   // 0xff
         }
         else {
            // DBGMSG("token: |%s|", token);
            DDCA_Trace_Group tg = trace_class_name_to_value(token);
            // DBGMSG("tg=0x%02x", tg);
            if (tg) {
               traceClasses |= tg;
            }
            else {
               fprintf(stderr, "Invalid trace group: %s\n", token);
               ok = false;
            }
        }
      }
      // DBGMSG("traceClasses = 0x%02x", traceClasses);
      parsed_cmd->traced_groups = traceClasses;
   }
// #endif

   if (trace_functions) {
      parsed_cmd->traced_functions = trace_functions;
   }
   if (trace_filenames) {
      parsed_cmd->traced_files = trace_filenames;
   }

   int rest_ct = 0;
   // don't pull debug into the if clause, need rest_ct to be set
   if (cmd_and_args) {
      for (; cmd_and_args[rest_ct] != NULL; rest_ct++) {
            DBGMSF(debug, "cmd_and_args[%d]: %s", rest_ct, cmd_and_args[rest_ct]);
      }
   }

   if (version_flag) {
      printf("ddcutil %s\n", BUILD_VERSION);
      // TODO: patch values at link time
      // printf("Built %s at %s\n", BUILD_DATE, BUILD_TIME);
#ifdef HAVE_ADL
      printf("Built with support for AMD Display Library (AMD proprietary driver).\n");
#else
      printf("Built without support for AMD Display Library (AMD proprietary driver).\n");
#endif
#ifdef USE_USB
      printf("Built with support for USB connected displays.\n");
#else
      printf("Built without support for USB connected displays.\n");
#endif
#ifdef ENABLE_FAILSIM
      printf("Built with function failure simulation.\n");
#else
      printf("Built without function failure simulation.\n");
#endif
#ifdef USE_LIBDRM
      printf("Built with libdrm services.\n");
#else
      printf("Built without libdrm services.\n");
#endif
      puts("");
      // if no command specified, include license in version information and terminate
      if (rest_ct == 0) {
         puts("Copyright (C) 2015-2020 Sanford Rockowitz");
         puts("License GPLv2: GNU GPL version 2 or later <http://gnu.org/licenses/gpl.html>");
         puts("This is free software: you are free to change and redistribute it.");
         puts("There is NO WARRANTY, to the extent permitted by law.");

         exit(0);
      }
   }

   // All options processed.  Check for consistency, set defaults
   if (explicit_display_spec_ct > 1) {
      fprintf(stderr, "Monitor specified in more than one way\n");
      free_display_identifier(parsed_cmd->pdid);
      parsed_cmd->pdid = NULL;
      ok = false;
   }
   // else if (explicit_display_spec_ct == 0)
   //   parsed_cmd->pdid = create_dispno_display_identifier(1);   // default monitor


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
            if (argctr > max_arg_ct) {
               fprintf(stderr, "Too many arguments\n");
               ok = false;
               break;
            }
            char * thisarg = (char *) cmd_and_args[argctr];
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

         if ( ok &&
              (parsed_cmd->cmd_id == CMDID_VCPINFO ||
               parsed_cmd->cmd_id == CMDID_GETVCP)
            )
         {
            Feature_Set_Ref * fsref = calloc(1, sizeof(Feature_Set_Ref));
            char * val = (parsed_cmd->argct > 0) ? parsed_cmd->args[0] : "ALL";
            ok = parse_feature_id_or_subset(val, parsed_cmd->cmd_id, fsref);
            DBGMSF(debug, "parse_feature_id_or_subset() returned: %d", ok);
            if (ok)
               parsed_cmd->fref = fsref;
            else
               fprintf(stderr, "Invalid feature code or subset: %s\n", parsed_cmd->args[0]);
         }

         // Ignore --notable for vcpinfo
         if ( ok && parsed_cmd->cmd_id  == CMDID_VCPINFO) {
            parsed_cmd->flags &= ~CMD_FLAG_NOTABLE;
         }

         if (ok && parsed_cmd->cmd_id == CMDID_GETVCP && (parsed_cmd->flags & CMD_FLAG_WO_ONLY) ) {
            fprintf(stdout, "Ignoring option --wo-only\n");
            parsed_cmd->flags &= ~CMD_FLAG_WO_ONLY;
         }

         if (ok && parsed_cmd->cmd_id == CMDID_SETVCP) {
            for (int argpos = 0; argpos < parsed_cmd->argct; argpos+=2) {
               // DBGMSG("argpos=%d, argct=%d", argpos, parsed_cmd->argct);
               if ( (argpos+1) == parsed_cmd->argct) {
                  fprintf(stderr, "Missing feature value\n");
                  ok = false;
                  break;
               }
               char * a1 = parsed_cmd->args[argpos+1];
               if ( streq(a1,"+") || streq(a1,"-") ) {
                  if ( (argpos+2) == parsed_cmd->argct) {
                       fprintf(stderr, "Missing relative feature value\n");
                       ok = false;
                       break;
                  }
                  char * a2 = parsed_cmd->args[argpos+2];
                  char * newval = calloc(1, 1 + strlen(a2) + 1);
                  strcpy(newval, a1);
                  strcat(newval, a2);
                  free(a1);
                  free(a2);
                  parsed_cmd->args[argpos+1] = newval;
                  int ndx = 0;
                  for (ndx = argpos+2; ndx < (parsed_cmd->argct - 1); ndx++) {
                     parsed_cmd->args[ndx] = parsed_cmd->args[ndx+1];
                  }
                  parsed_cmd->args[parsed_cmd->argct - 1] = NULL;
                  parsed_cmd->argct--;
               }
            }
         }
      }  // recognized command
   }

   if (ok)
      ok = validate_output_level(parsed_cmd);

   DBGMSF(debug, "Calling g_option_context_free(), context=%p...", context);
   g_option_context_free(context);
   // if (trace_classes) {
   //    DBGMSG("Freeing trace_classes=%p", trace_classes);
   //    free(trace_classes);   // trying to avoid valgrind error re g_option_context_parse() - doesn't solve it
   // }

   if (debug || debug_parse_flag) {
      DBGMSG("ok=%s", sbool(ok));
      dbgrpt_parsed_cmd(parsed_cmd, 0);
   }

   if (!ok) {
      free_parsed_cmd(parsed_cmd);
      parsed_cmd = NULL;
   }

   DBGMSF(debug, "Returning: %p", parsed_cmd);
   return parsed_cmd;
}
