/** @file cmd_parser_goption.c
 *
 *  Parse the command line using the glib goption functions.
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config.h>

#include <assert.h>
#include <config.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util/coredefs.h"
#include "util/debug_util.h"
#include "util/error_info.h"
#include "util/string_util.h"
#include "util/report_util.h"

#include "base/build_info.h"
#include "base/core.h"
#include "base/displays.h"
#include "base/parms.h"
#include "base/trace_control.h"

#include "cmdline/cmd_parser_aux.h"
#include "cmdline/cmd_parser.h"
#include "cmdline/parsed_cmd.h"


// Variables used by callback functions
static char *            usbwork        = NULL;
static DDCA_Output_Level output_level   = DDCA_OL_NORMAL;
static DDCA_Stats_Type   stats_work     = DDCA_STATS_NONE;
static Cache_Types       discarded_caches_work = NO_CACHES;
static bool              verbose_stats  = false;
static bool              internal_stats = false;
static Bit_Set_32        ignored_hiddev_work = 0;    // gcc claims not const??? EMPTY_BIT_SET_32;


// Callback function for processing --terse, --verbose and synonyms
static gboolean
output_arg_func(const gchar* option_name,
                const gchar* value,
                gpointer     data,
                GError**     error)
{
   bool debug = false;
   DBGMSF(debug, "option_name=|%s|, value|%s|, data=%p", option_name, value, data);
   bool ok = true;

   if (      streq(option_name, "-v") || streq(option_name, "--verbose") )
      output_level = DDCA_OL_VERBOSE;
   else if ( streq(option_name, "-t")  || streq(option_name, "--terse") || streq(option_name, "--brief") )
      output_level = DDCA_OL_TERSE;
   else if ( streq(option_name, "--vv") || streq(option_name, "--very-verbose") )
      output_level = DDCA_OL_VV;
   else {
      PROGRAM_LOGIC_ERROR("Unexpected option_name: %s", option_name);
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                  "PROGRAM LOGIC ERROR: Unexpected option_name: %s", option_name);
      ok = false;
   }

   return ok;
}


// Callback function for processing --stats, --vstats, --istats
static gboolean
stats_arg_func(const    gchar* option_name,
               const    gchar* value,
               gpointer data,
               GError** error)
{
   bool debug = false;
   DBGMSF(debug,"option_name=|%s|, value|%s|, data=%p", option_name, value, data);

   if (streq(option_name, "--vstats"))
      verbose_stats = true;
   else if (streq(option_name, "--istats")) {
      verbose_stats = true;
      internal_stats = true;
   }

   bool ok = true;
   if (value) {
      char * v2 = strdup_uc(value);
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



// Callback function for processing --discard-cache
static gboolean
discard_cache_arg_func(
               const    gchar* option_name,
               const    gchar* value,
               gpointer data,
               GError** error)
{
   bool debug = false;
   DBGMSF(debug,"option_name=|%s|, value|%s|, data=%p", option_name, value, data);

   bool ok = true;
   if (value) {
      char * v2 = strdup_uc(value);
      if ( streq(v2,"ALL") ) {
         discarded_caches_work |= ALL_CACHES;
      }
      else if ( is_abbrev(v2, "CAPABILITIES",3)) {
         discarded_caches_work |= CAPABILITIES_CACHE;
      }
      else if (streq(v2,"DSA") || is_abbrev(v2, "SLEEP",3)) {
         discarded_caches_work |= DSA2_CACHE;
      }
      else
         ok = false;
      free(v2);
   }
   else {
      discarded_caches_work = ALL_CACHES;
   }

   if (!ok) {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "invalid cache type: %s", value );
   }
   return ok;
}


#ifdef ENABLE_USB
static gboolean
ignored_hiddev_arg_func(const    gchar* option_name,
               const    gchar* value,
               gpointer data,
               GError** error)
{
   bool debug = false;
   DBGMSF(debug,"option_name=|%s|, value|%s|, data=%p", option_name, value, data);

   int ival;
   bool ok =  str_to_int(value, &ival, 10);
   // DBGMSG("ival=%d", ival);
   if (!ok || ival < 0 || ival >= BIT_SET_32_MAX) {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "Invalid hiddev bus number: %s", value);
      ok = false;
   }
   else
      ignored_hiddev_work = bs32_insert(ignored_hiddev_work, ival);
   // DBGMSF(debug, "Returning %s", sbool(ok));
   return ok;
}
#endif


// #define FUTURE
#ifdef FUTURE
   gboolean debug_pre_parse_func(
                        GOptionContext *context,
                        GOptionGroup *group,
                        gpointer data,
                        GError **error)
   {
      DBGMSG("Executing");
     return true;
   }

   gboolean debug_post_parse_func(
                        GOptionContext *context,
                        GOptionGroup *group,
                        gpointer data,
                        GError **error)
   {
      DBGMSG("Executing");
     return true;
   }
#endif


static void emit_parser_error(GPtrArray* errmsgs, const char * func, const char * msg, ...) {
   va_list(args);
   va_start(args, msg);
   char * buffer = g_strdup_vprintf(msg, args);
   va_end(args);

   // Hack to clean up msgs still having \n at end
   if (strlen(buffer) > 1 && buffer[strlen(buffer)-1] == '\n')
      buffer[strlen(buffer)-1] = '\0';

   if (errmsgs) {
      g_ptr_array_add(errmsgs, g_strdup(buffer));
   }
   else{
      fprintf(stderr, "%s\n", buffer);
   }
   free(buffer);
}

#define EMIT_PARSER_ERROR(errmsgs, msg, ...) \
   emit_parser_error(errmsgs, __func__, msg, ##__VA_ARGS__)


static bool parse_maxtrywork(char * maxtrywork, Parsed_Cmd * parsed_cmd, GPtrArray* errmsgs) {
    bool debug = false;
    DBGMSF(debug, "retrywork, argument = |%s|", maxtrywork );
    bool parsing_ok = true;

    Null_Terminated_String_Array pieces = strsplit(maxtrywork, ",");
    int ntsal = ntsa_length(pieces);
    DBGMSF(debug, "ntsal=%d", ntsal );
    if (ntsa_length(pieces) != 3) {
       EMIT_PARSER_ERROR(errmsgs,  "Option --maxtries requires 3 values");
       parsing_ok = false;
    }
    else {
       int ndx = 0;
       char trimmed_piece[10];
       for (; pieces[ndx] != NULL; ndx++) {
          char * token = strtrim_r(pieces[ndx], trimmed_piece, 10);
          if (strlen(token) > 0 && !streq(token,".")) {
             int ival;
             int ct = sscanf(token, "%ud", &ival);
             if (ct != 1) {
                EMIT_PARSER_ERROR(errmsgs,  "Invalid --maxtries value: %s", token);
                parsing_ok = false;
             }
             else if (ival > MAX_MAX_TRIES) {
                EMIT_PARSER_ERROR(errmsgs,  "--maxtries value %d exceeds %d", ival, MAX_MAX_TRIES);
                parsing_ok = false;
             }
             else if (ival < 0) {
                EMIT_PARSER_ERROR(errmsgs,  "negative --maxtries value: %d", ival);
                parsing_ok = false;
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

    DBGMSF(debug, "maxtries = %d,%d,%d", parsed_cmd->max_tries[0],
                                         parsed_cmd->max_tries[1],
                                         parsed_cmd->max_tries[2]);
    DBGMSF(debug, "returning %s", sbool(parsing_ok));
    return parsing_ok;
}


static bool parse_display_identifier(
      Parsed_Cmd *  parsed_cmd,
      GPtrArray *   errmsgs,
      int           dispwork,
      int           buswork,
      int           hidwork,
      char *        usbwork,
      char *        edidwork,
      char *        mfg_id_work,
      char *        modelwork,
      char *        snwork)
{
   bool parsing_ok = true;
   int  explicit_display_spec_ct = 0;

   if (usbwork) {
#ifdef ENABLE_USB
      bool debug = false;
      DBGMSF(debug, "usbwork = |%s|", usbwork);
      int busnum;
      int devicenum;
      bool arg_ok = parse_dot_separated_arg(usbwork, &busnum, &devicenum);
      if (!arg_ok)
         arg_ok = parse_colon_separated_arg(usbwork, &busnum, &devicenum);
      if (!arg_ok) {
         EMIT_PARSER_ERROR(errmsgs, "Invalid USB argument: %s", usbwork );
         parsing_ok = false;
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
      EMIT_PARSER_ERROR(errmsgs,
            "ddcutil not built with support for USB connected monitors.  --usb option invalid.");
      parsing_ok = false;
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
#ifdef ENABLE_USB
      // avoid memory leak in case parsed_cmd->pdid set in more than 1 way
      if (parsed_cmd->pdid)
         free_display_identifier(parsed_cmd->pdid);
      parsed_cmd->pdid = create_usb_hiddev_display_identifier(hidwork);
      explicit_display_spec_ct++;
#else
      EMIT_PARSER_ERROR(errmsgs,
            "ddcutil not built with support for USB connected monitors.  --hid option invalid.");
      parsing_ok = false;
#endif
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
         EMIT_PARSER_ERROR(errmsgs,  "EDID hex string not 256 characters");
         parsing_ok = false;
      }
      else {
         Byte * pba = NULL;
         int bytect = hhs_to_byte_array(edidwork, &pba);
         if (bytect < 0 || bytect != 128) {
            EMIT_PARSER_ERROR(errmsgs,  "Invalid EDID hex string");
            parsing_ok = false;
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

   if (explicit_display_spec_ct > 1) {
      EMIT_PARSER_ERROR(errmsgs, "Monitor specified in more than one way");
      free_display_identifier(parsed_cmd->pdid);
      parsed_cmd->pdid = NULL;
      parsing_ok = false;
   }

   return parsing_ok;
}


static bool parse_mccswork(char * mccswork, Parsed_Cmd * parsed_cmd, GPtrArray * errmsgs) {
   bool debug = false;
   bool arg_ok = false;
   if (mccswork) {
      DBGMSF(debug, "mccswork = |%s|", mccswork);
      DDCA_MCCS_Version_Spec vspec = parse_vspec(mccswork);
      if (!vcp_version_eq(vspec, DDCA_VSPEC_UNKNOWN)) {
         arg_ok = vcp_version_is_valid(vspec, false);
      }
      if (!arg_ok) {
         EMIT_PARSER_ERROR(errmsgs, "Invalid MCCS spec: %s", mccswork );
         EMIT_PARSER_ERROR(errmsgs, "Valid MCCS versions are: %s", valid_vcp_versions);
      }
      else {
         parsed_cmd->mccs_vspec = vspec;
         // parsed_cmd->mccs_version_id = mccs_version_spec_to_id(vspec);
      }
   }
   return arg_ok;
}


static bool parse_int_work(char * sval, int * result_loc, GPtrArray * errmsgs) {
   bool debug = false;
   bool ok = true;
   DBGMSF(debug, "sval: %s", sval);
   if (sval) {
     ok = str_to_int(sval, result_loc, 0);
     if (!ok)
        EMIT_PARSER_ERROR(errmsgs,  "Invalid integer or hex number: %s", sval);
   }
   DBGMSF(debug, "Done.  Returning: %s. result_loc -> %d (0x%08x)",
         sbool(ok), *result_loc, *result_loc);
   return ok;
}


static bool parse_sleep_multiplier(
      const char*  sval,
      float *      result_loc,
      GPtrArray*   errmsgs)
{
   bool debug = false;

   bool arg_ok = false;
   if (sval) {
      DBGMSF(debug, "sval = |%s|", sval);
      float multiplier = 0.0f;
      arg_ok = str_to_float(sval, &multiplier);
      if (arg_ok) {
         if (multiplier < 0.0f || multiplier >= 100.0)
            arg_ok = false;
      }

      if (arg_ok) {
         *result_loc = multiplier;
      }
      else {
         EMIT_PARSER_ERROR(errmsgs, "Invalid sleep-multiplier: %s", sval );
      }
   }
   return arg_ok;
}


#ifdef OLD
static bool parse_sleep_multiplier0(
      const char*  sleep_multiplier_work,
      Parsed_Cmd*  parsed_cmd,
      GPtrArray*   errmsgs)
{
   bool debug = false;
   bool arg_ok = false;

   if (sleep_multiplier_work) {
      DBGMSF(debug, "sleep_multiplier_work = |%s|", sleep_multiplier_work);
      float multiplier = 0.0f;
      arg_ok = str_to_float(sleep_multiplier_work, &multiplier);
      if (arg_ok) {
         if (multiplier < 0.0f || multiplier >= 100.0)
            arg_ok = false;
         // if (parsed_cmd->parser_mode == MODE_DDCUTIL && multiplier == 0.0f)
         //    arg_ok = false;
      }

      if (!arg_ok) {
         EMIT_PARSER_ERROR(errmsgs, "Invalid sleep-multiplier: %s", sleep_multiplier_work );
      }
      else {
         parsed_cmd->sleep_multiplier = multiplier;
         parsed_cmd->flags |= CMD_FLAG_EXPLICIT_SLEEP_MULTIPLIER;
      }
   }
   return arg_ok;
}
#endif


static void unhide_options(GOptionEntry * options) {
   GOptionEntry * cur = options;
   for (;cur->long_name;  cur++) {
      // DBGMSG("cur=%p, cur->long_name=%p - %s", cur, cur->long_name, cur->long_name);
      cur->flags = cur->flags & ~G_OPTION_FLAG_HIDDEN;
   }
}


static bool parse_trace_classes(gchar** trace_classes, Parsed_Cmd* parsed_cmd, GPtrArray* errmsgs)
{
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
                parsing_ok = false;
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

   bool parsing_ok = true;
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
               EMIT_PARSER_ERROR(errmsgs,  "Invalid trace group: %s\n", token);
               parsing_ok = false;
            }
        }
      }
      // DBGMSG("traceClasses = 0x%02x", traceClasses);
      parsed_cmd->traced_groups = traceClasses;
   }
   return parsing_ok;
}


bool parse_syslog_level(
      const char *        sval,
      DDCA_Syslog_Level * result_loc,
      GPtrArray *         errmsgs)
{
   assert(sval);
   assert(result_loc);
   bool debug = false;
   bool parsing_ok = true;
   DBGF(debug, "sval=|%s|", sval);

   *result_loc = syslog_level_name_to_value(sval);
   if (*result_loc == DDCA_SYSLOG_NOT_SET) {
      parsing_ok = false;
      EMIT_PARSER_ERROR(errmsgs, "Invalid syslog level: %s", sval );
      EMIT_PARSER_ERROR(errmsgs, "Valid values are %s", valid_syslog_levels_string);
   }
   if (debug)
      printf("(%s) Returning %s, *result_loc = %d\n",
            __func__, sbool(parsing_ok), *result_loc);
   return parsing_ok;
}


static bool parse_setvcp_args(Parsed_Cmd * parsed_cmd, GPtrArray* errmsgs) {
   bool parsing_ok = true;
   // for (int argpos = 0; argpos < parsed_cmd->argct; argpos+=2) {
   int argpos = 0;
   while(argpos < parsed_cmd->argct) {
      Parsed_Setvcp_Args psv;
      bool feature_code_ok = any_one_byte_hex_string_to_byte_in_buf(
                    parsed_cmd->args[argpos],
                    &psv.feature_code);
      if (!feature_code_ok) {
         EMIT_PARSER_ERROR(errmsgs, "Invalid feature code: %s", parsed_cmd->args[argpos]);
         parsing_ok = false;
         break;
      }
      argpos++;
      if (argpos >= parsed_cmd->argct) {
         EMIT_PARSER_ERROR(errmsgs, "Missing feature value");
         parsing_ok = false;
         break;
      }
      psv.feature_value_type = VALUE_TYPE_ABSOLUTE;
      if ( streq(parsed_cmd->args[argpos], "+") || streq(parsed_cmd->args[argpos], "-") )
      {
         if ( streq(parsed_cmd->args[argpos], "+") )
            psv.feature_value_type = VALUE_TYPE_RELATIVE_PLUS;
         else
            psv.feature_value_type = VALUE_TYPE_RELATIVE_MINUS;
         argpos++;
         if (argpos >= parsed_cmd->argct) {
            EMIT_PARSER_ERROR(errmsgs,  "Missing feature value");
            parsing_ok = false;
            break;
         }
      }
      psv.feature_value = g_strdup(parsed_cmd->args[argpos]);
      g_array_append_val(parsed_cmd->setvcp_values, psv);
      argpos++;
   }
   return parsing_ok;
}

static bool parse_discard_args(Parsed_Cmd * parsed_cmd, GPtrArray* errmsgs) {
   bool parsing_ok = true;
   assert(parsed_cmd->argct == 1 || parsed_cmd->argct == 2);
   strupper(parsed_cmd->args[0]);
   if (parsed_cmd->argct == 2)
      strupper(parsed_cmd->args[1]);

   if (parsed_cmd->argct == 1) {
      if ( is_abbrev(parsed_cmd->args[0], "CACHES", 5) )
         parsed_cmd->discarded_cache_types = ALL_CACHES;
      else
         parsing_ok = false;
   }

   else {
      if ( !is_abbrev(parsed_cmd->args[1], "CACHES", 5) ) {
         parsing_ok = false;
      }
      else {
         if (is_abbrev(parsed_cmd->args[0], "CAPABILITIES", 3) )
            parsed_cmd->discarded_cache_types = CAPABILITIES_CACHE;
#ifdef REMOVED
         else if (is_abbrev(parsed_cmd->args[0], "DISPLAYS", 3) )
            parsed_cmd->cache_types = DISPLAYS_CACHE;
#endif
         else if (is_abbrev(parsed_cmd->args[0], "DSA", 3) )
            parsed_cmd->discarded_cache_types = DSA2_CACHE;
         else if (is_abbrev(parsed_cmd->args[0], "ALL", 3) )
            parsed_cmd->discarded_cache_types = ALL_CACHES;
         else
            parsing_ok = false;
      }
   }

   if (!parsing_ok) {
      char * s = g_strdup_printf("%s %s",
                           parsed_cmd->args[0],
                           (parsed_cmd->argct > 1) ? parsed_cmd->args[1] : "");
      EMIT_PARSER_ERROR(errmsgs, "Unrecognized DISCARD argument(s): %s", s);
      g_free(s);
   }

   return parsing_ok;
}


static void report_ddcutil_build_info() {

      // TODO: patch values at link time
      // printf("Built %s at %s\n", BUILD_DATE, BUILD_TIME);
#ifdef ENABLE_USB
      printf("Built with support for displays using USB for MCCS communication.\n");
#else
      printf("Built without support for displays using USB for MCCS communication.\n");
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
}


#ifdef UNUSED
Preparsed_Cmd * preparse_command(
      int         argc,
      char *      argv[],
      Parser_Mode parser_mode,
      GPtrArray * errmsgs)
{
   bool debug = true;
   char * s = getenv("DDCUTIL_DEBUG_PARSE");
   if (s && strlen(s) > 0)
      debug = true;
   DBGMSF(debug, "Starting. parser_mode = %d", parser_mode );
   assert(parser_mode == MODE_DDCUTIL);
   if (debug) {
      DBGMSG("argc=%d", argc);
      int ndx = 0;
      for (; ndx < argc; ndx++) {
         DBGMSG("argv[%d] = |%s|", ndx, argv[ndx]);
      }
   }

   Preparsed_Cmd * ppc = calloc(1, sizeof(Preparsed_Cmd));
   ppc->severity = DDCA_SYSLOG_NOT_SET;
   ppc->verbose = false;
   ppc->noconfig = false;

   char * syslog_work = NULL;
   char ** ignored = NULL;

   GOptionEntry preparser_options[] = {
         {"verbose",   'v', 0, G_OPTION_ARG_NONE,         &ppc->verbose,  NULL, NULL},
         {"noconfig", '\0', 0, G_OPTION_ARG_NONE,         &ppc->noconfig, NULL, NULL},

         {"syslog",   '\0', 0, G_OPTION_ARG_STRING,       &syslog_work,   NULL, NULL},
         {G_OPTION_REMAINING,
                      '\0', 0, G_OPTION_ARG_STRING_ARRAY, &ignored,       NULL, NULL},
         {NULL},
   };

   GOptionGroup * all_options = g_option_group_new(
         "group name", "group description", "help description", NULL, NULL);
   g_option_group_add_entries(all_options, preparser_options);


   GError* error = NULL;
   GOptionContext* context  = g_option_context_new("- Preparser");
   g_option_context_set_main_group(context, all_options);
   g_option_context_set_help_enabled(context, false);
   // Pass a mangleable copy of argv to g_option_context_parse_strv().
   Null_Terminated_String_Array temp_argv = ntsa_copy(argv, true);
   bool parsing_ok = g_option_context_parse_strv(context, &temp_argv, &error);
   if (ignored) {
      ntsa_show(ignored);
      ntsa_free(ignored, true);
   }
   if (!parsing_ok) {
      char * mode_name = (parser_mode == MODE_DDCUTIL) ? "ddcutil" : "libddcutil";
      if (error) {
         // EMIT_PARSER_ERROR(errmsgs,  "%s option parsing failed: %s", mode_name, error->message);
         emit_parser_error(errmsgs,  __func__, "%s", error->message);
      }
      else
         emit_parser_error(errmsgs,  __func__,  "%s option parsing failed", mode_name);
   }
   ntsa_free(temp_argv, true);

   if (syslog_work) {
      DDCA_Syslog_Level level;
      bool this_ok = parse_syslog_level(syslog_work, &level, errmsgs);
      // printf("(%s) this_ok = %s\n", __func__, sbool(this_ok));
      if (this_ok)
         ppc->severity = level;
      else
         parsing_ok = false;
   }
   if (!parsing_ok) {
      free(ppc);
      ppc = NULL;
   }


   return ppc;
}
#endif


/* Primary parsing function
 *
 * \param   argc         number of command line arguments
 * \param   argv         array of pointers to command line arguments
 * \param   parser_mode  indicate whether called for ddcutil or libddcutil
 * \param   errmsgs      if non-null collect error messages, do not write to terminal
 * \return  pointer to newly allocated Parsed_Cmd struct if parsing successful
 *          NULL if execution should be terminated
 */
Parsed_Cmd *
parse_command(
      int         argc,
      char *      argv[],
      Parser_Mode parser_mode,
      GPtrArray * errmsgs)
{
   bool debug = false;
   char * s = getenv("DDCUTIL_DEBUG_PARSE");
   if (s && strlen(s) > 0)
      debug = true;
   DBGMSF(debug, "Starting. parser_mode = %d", parser_mode );
#ifndef NDEBUG
   init_cmd_parser_base();   // assertions
#endif

   if (debug) {
      DBGMSG("argc=%d", argc);
      int ndx = 0;
      for (; ndx < argc; ndx++) {
         DBGMSG("argv[%d] = |%s|", ndx, argv[ndx]);
      }
   }

   Parsed_Cmd * parsed_cmd = new_parsed_cmd();
   parsed_cmd->parser_mode = parser_mode;
   // parsed_cmd->pdid = create_dispno_display_identifier(1);   // default monitor
   // DBGMSG("After new_parsed_cmd(), parsed_cmd->output_level_name = %s", output_level_name(parsed_cmd->output_level));

   gchar * original_command = g_strjoinv(" ",argv);
   DBGMSF(debug, "original command: %s", original_command);
   parsed_cmd->raw_command = original_command;

// gboolean stats_flag       = false;
   gboolean ddc_flag         = false;
   gboolean force_flag       = false;
   gboolean allow_unrecognized_feature_flag = false;
   gboolean force_slave_flag = false;
   gboolean show_unsupported_flag = false;
   gboolean version_flag     = false;
   gboolean timestamp_trace_flag = false;
   gboolean wall_timestamp_trace_flag = false;
   gboolean thread_id_trace_flag = false;
   gboolean process_id_trace_flag = false;
   gboolean verify_flag    = false;
   gboolean noverify_flag  = false;
   gboolean async_flag     = false;
   // gboolean async_check_i2c_flag = true;
   gboolean report_freed_excp_flag = false;
   gboolean notable_flag   = true;
   gboolean rw_only_flag   = false;
   gboolean ro_only_flag   = false;
   gboolean wo_only_flag   = false;
   gboolean enable_udf_flag = DEFAULT_ENABLE_UDF;
   const char * enable_udf_expl  = (enable_udf_flag) ? "Enable User Defined Features (default)" : "Enable User Defined Features";
   const char * disable_udf_expl = (enable_udf_flag) ? "Disable User Defined Features" : "Disable User Defined Features (default)";
#ifdef ENABLE_USB
   gboolean enable_usb_flag = DEFAULT_ENABLE_USB;
   const char * enable_usb_expl  = (enable_usb_flag) ? "Detect USB devices (default)" : "Detect USB devices";
   const char * disable_usb_expl = (enable_usb_flag) ? "Ignore USB devices" : "Ignore USB devices (default)";
   gchar**  ignored_vid_pid  = NULL;
#endif
   gboolean timeout_i2c_io_flag = false;
   gboolean reduce_sleeps_specified = false;
   gboolean deferred_sleep_flag = false;
   gboolean show_settings_flag = false;
   gboolean i2c_io_fileio_flag = false;
   gboolean i2c_io_ioctl_flag  = false;
   gboolean debug_parse_flag   = false;
   gboolean parse_only_flag    = false;
   gboolean x52_no_fifo_flag   = false;
   gboolean enable_dsa2_flag   = DEFAULT_ENABLE_DSA2;
   // int      i2c_bus_check_async_min = DEFAULT_I2C_BUS_CHECK_ASYNC_MIN;
   // int      ddc_check_async_min = DEFAULT_DDC_CHECK_ASYNC_MIN;
   char     i2c_bus_check_async_expl[80];
   g_snprintf(i2c_bus_check_async_expl, 80, "Threshold for parallel examination of I2C buses (Experimental). Default=%d.",
         DEFAULT_BUS_CHECK_ASYNC_THRESHOLD);
   char     ddc_check_async_expl[80];
   g_snprintf(ddc_check_async_expl, 80, "Threshold for parallel examination of possible DDC devices. Default=%d.",
         DEFAULT_DDC_CHECK_ASYNC_THRESHOLD);
   const char * enable_dsa2_expl  = (enable_dsa2_flag) ? "Enable dynamic sleep algorithm (default)" : "Enable dynamic sleep algorithm";
   const char * disable_dsa2_expl = (enable_dsa2_flag) ? "Disable dynamic sleep algorithm" : "Disable dynamic sleep algorithm (default)";

   gboolean enable_cc_flag = DEFAULT_ENABLE_CACHED_CAPABILITIES;
   const char * enable_cc_expl =  (enable_cc_flag) ? "Enable cached capabilities (default)" : "Enable cached capabilities";
   const char * disable_cc_expl = (enable_cc_flag) ? "Disable cached capabilities" : "Disable cached capabilities (default)";

// #ifdef REMOVED
   gboolean enable_cd_flag = DEFAULT_ENABLE_CACHED_DISPLAYS;
   const char * enable_cd_expl =  (enable_cd_flag) ? "Enable cached displays (default)" : "Enable cached displays";
   const char * disable_cd_expl = (enable_cd_flag) ? "Disable cached displays" : "Disable cached displays (default)";
// #endif

   gboolean enable_flock_flag = DEFAULT_ENABLE_FLOCK;
   const char * enable_flock_expl =  (enable_flock_flag) ? "Enable cross-instance locking (default)" : "Enable cross-instance locking";
   const char * disable_flock_expl = (enable_flock_flag) ? "Disable cross-instance locking" : "Disable cross-instance locking (default)";

   gboolean quick_flag         = false;
   gboolean mock_data_flag     = false;
   gboolean profile_api_flag   = false;
   gboolean null_msg_for_unsupported_flag = false;
   gboolean enable_heuristic_unsupported_flag = true;

   char *   mfg_id_work     = NULL;
   char *   modelwork       = NULL;
   char *   snwork          = NULL;
   char *   edidwork        = NULL;

   char *   mccswork        = NULL;   // MCCS version
// // char *   tracework       = NULL;
   char**   cmd_and_args    = NULL;
   gchar**  trace_classes   = NULL;
   DDCA_Syslog_Level syslog_level = (parser_mode == MODE_DDCUTIL) ? DEFAULT_DDCUTIL_SYSLOG_LEVEL
                                                                  : DEFAULT_LIBDDCUTIL_SYSLOG_LEVEL;
   char *   syslog_work     = NULL;
   gint     buswork         = -1;
   gint     hidwork         = -1;
   gint     dispwork        = -1;
   char *   maxtrywork      = NULL;
// char *   trace_destination = NULL;
   gboolean trace_to_syslog_only_flag = false;
   gboolean stats_to_syslog_only_flag = false;
   gint     edid_read_size_work = -1;
   gboolean watch_displays_flag = false;

   gboolean try_get_edid_from_sysfs = DEFAULT_TRY_GET_EDID_FROM_SYSFS;

   char *   enable_tgefs_expl = NULL;
   char *  disable_tgefs_expl = NULL;
   if (DEFAULT_TRY_GET_EDID_FROM_SYSFS) {
      enable_tgefs_expl = "get EDID from /sys when possible (default)";
      disable_tgefs_expl = "do not try to get EDID from /sys";
   }
   else {
      enable_tgefs_expl = "get EDID from /sys when possible";
      disable_tgefs_expl = "do not try to get EDID from /sys (default)";
   }

   gboolean f1_flag         = false;
   gboolean f2_flag         = false;
   gboolean f3_flag         = false;
   gboolean f4_flag         = false;
   gboolean f5_flag         = false;
   gboolean f6_flag         = false;
   gboolean f7_flag         = false;
   gboolean f8_flag         = false;
   gboolean f9_flag         = false;
   gboolean f10_flag        = false;
   gboolean f11_flag        = false;
   gboolean f12_flag        = false;
   gboolean f13_flag        = false;
   gboolean f14_flag        = false;
   char *   i1_work         = NULL;
   char *   i2_work         = NULL;
   char *   i3_work         = NULL;
   char *   i4_work         = NULL;
   char *   i5_work         = NULL;
   char *   i6_work         = NULL;
   char *   i7_work         = NULL;
   char *   i8_work         = NULL;
   char *   fl1_work        = NULL;
   char *   fl2_work        = NULL;
   char *   failsim_fn_work = NULL;
   // gboolean enable_failsim_flag = false;
   char *   sleep_multiplier_work = NULL;
   char *   min_dynamic_sleep_work = NULL;
   char *   i2c_source_addr_work = NULL;
   gboolean skip_ddc_checks_flag = false;

   gboolean hidden_help_flag = false;
   gboolean disable_config_flag = false;

   GOptionEntry preparser_options[] = {
         {"hh",         '\0', 0, G_OPTION_ARG_NONE, &hidden_help_flag, "Show hidden options", NULL},
         {NULL},
   };

#ifdef OLD
   GOptionEntry libddcutil_only_options[] = {
         {NULL},
   };
#endif

   GOptionEntry ddcutil_only_options[] = {
         //  Monitor selection options
         {"display", 'd',  0, G_OPTION_ARG_INT,      &dispwork,         "Display number",              "number"},
         {"dis",    '\0',  0, G_OPTION_ARG_INT,      &dispwork,         "Display number",              "number"},
         {"bus",     'b',  0, G_OPTION_ARG_INT,      &buswork,          "I2C bus number",              "busnum" },
         {"hiddev", '\0',  0, G_OPTION_ARG_INT,      &hidwork,          "hiddev device number",        "number" },
         {"usb",     'u',  0, G_OPTION_ARG_STRING,   &usbwork,          "USB bus and device numbers",  "busnum.devicenum"},
         {"mfg",     'g',  0, G_OPTION_ARG_STRING,   &mfg_id_work,      "Monitor manufacturer code",   "mfg_id"},
         {"model",   'l',  0, G_OPTION_ARG_STRING,   &modelwork,        "Monitor model",               "model name"},
         {"sn",      'n',  0, G_OPTION_ARG_STRING,   &snwork,           "Monitor serial number",       "serial number"},
         {"edid",    'e',  0, G_OPTION_ARG_STRING,   &edidwork,         "Monitor EDID",            "256 char hex string" },

         // Feature selection filters
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

         // Output control
         {"verbose", 'v',  G_OPTION_FLAG_NO_ARG,
                              G_OPTION_ARG_CALLBACK, output_arg_func,   "Show extended detail",             NULL},
         {"terse",   't',  G_OPTION_FLAG_NO_ARG,
                              G_OPTION_ARG_CALLBACK, output_arg_func,   "Show brief detail",                NULL},
         {"brief",   '\0', G_OPTION_FLAG_NO_ARG,
                              G_OPTION_ARG_CALLBACK, output_arg_func,   "Show brief detail",                NULL},
         {"vv",      '\0', G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN,
                              G_OPTION_ARG_CALLBACK, output_arg_func,   "Show extra verbose detail",        NULL},
         {"very-verbose", '\0', G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN,
                              G_OPTION_ARG_CALLBACK, output_arg_func,   "Show extra verbose detail",        NULL},

         // Program information
         {"settings",'\0', 0, G_OPTION_ARG_NONE,     &show_settings_flag,"Show current settings",           NULL},
         {"version", 'V',  0, G_OPTION_ARG_NONE,     &version_flag,     "Show ddcutil version",             NULL},

         // Miscellaneous
         // move to preparser_options if also implemented for libddcutil
         {"noconfig",'\0', 0, G_OPTION_ARG_NONE,     &disable_config_flag, "Do not process configuration file", NULL},

      {NULL},
   };

   GOptionEntry common_options[] = {
   //  long_name short flags option-type          gpointer           description                    arg description

      // Diagnostic output
      {"ddc",     '\0', 0, G_OPTION_ARG_NONE,     &ddc_flag,         "Report DDC protocol and data errors (Deprecated, use --ddcdata)", NULL},
      {"ddcdata", '\0', 0, G_OPTION_ARG_NONE,     &ddc_flag,         "Report DDC protocol and data errors", NULL},
      {"stats",   's',  G_OPTION_FLAG_OPTIONAL_ARG,
                           G_OPTION_ARG_CALLBACK, stats_arg_func,    "Show performance statistics",  "stats type"},
      {"vstats",  '\0', G_OPTION_FLAG_OPTIONAL_ARG,
                           G_OPTION_ARG_CALLBACK, stats_arg_func,    "Show detailed performance statistics",  "stats type"},
      {"istats",  '\0', G_OPTION_FLAG_OPTIONAL_ARG,
                                                G_OPTION_ARG_CALLBACK, stats_arg_func,    "Show detailed and internal performance statistics",  "stats type"},
      {"profile-api",'\0', 0, G_OPTION_ARG_NONE, &profile_api_flag,      "Profile API calls", NULL},
      {"syslog",      '\0',0, G_OPTION_ARG_STRING,       &syslog_work,                    "system log level", valid_syslog_levels_string},


      // Performance
      {"enable-capabilities-cache",
                  '\0', 0, G_OPTION_ARG_NONE,     &enable_cc_flag,   enable_cc_expl,     NULL},
      {"disable-capabilities-cache", '\0', G_OPTION_FLAG_REVERSE,
                           G_OPTION_ARG_NONE,     &enable_cc_flag,   disable_cc_expl ,   NULL},

      {"enable-displays-cache",
                   '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,     &enable_cd_flag,   enable_cd_expl,     NULL},
      {"disable-displays-cache", '\0', G_OPTION_FLAG_REVERSE|G_OPTION_FLAG_HIDDEN,
                            G_OPTION_ARG_NONE,     &enable_cd_flag,   disable_cd_expl ,   NULL},

      {"sleep-multiplier", '\0', 0,
                            G_OPTION_ARG_STRING,   &sleep_multiplier_work, "Multiplication factor for DDC sleeps", "number"},

      {"enable-dynamic-sleep",    '\0', 0, G_OPTION_ARG_NONE, &enable_dsa2_flag, enable_dsa2_expl,  NULL},
      {"disable-dynamic-sleep",   '\0', G_OPTION_FLAG_REVERSE,
                                           G_OPTION_ARG_NONE, &enable_dsa2_flag, disable_dsa2_expl, NULL},
      {"dynamic-sleep-adjustment",'\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &enable_dsa2_flag, enable_dsa2_expl,  NULL},
      {"dsa",                     '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &enable_dsa2_flag, enable_dsa2_expl,  NULL},

      {"nodsa",                   '\0', G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_REVERSE,
                                            G_OPTION_ARG_NONE, &enable_dsa2_flag, disable_dsa2_expl, NULL},

      {"enable-dsa",              '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &enable_dsa2_flag, enable_dsa2_expl,  NULL},
      {"disable-dsa",             '\0', G_OPTION_FLAG_HIDDEN |G_OPTION_FLAG_REVERSE,
                                            G_OPTION_ARG_NONE, &enable_dsa2_flag, disable_dsa2_expl, NULL},
      {"dsa2",                    '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &enable_dsa2_flag, enable_dsa2_expl,  NULL},
      {"disable-dsa2",            '\0', G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_REVERSE,
                                            G_OPTION_ARG_NONE, &enable_dsa2_flag, disable_dsa2_expl, NULL},
      {"min-dynamic-multiplier", '\0', 0,
                                  G_OPTION_ARG_STRING,  &min_dynamic_sleep_work, "Lowest allowed dynamic sleep multiplier", "number"},
      {"min-dynamic-sleep-multiplier", '\0', G_OPTION_FLAG_HIDDEN,
         G_OPTION_ARG_STRING,  &min_dynamic_sleep_work, "Lowest allowed dynamic sleep multiplier", "number"},

#ifdef OUT
      {"enable-async-ddc-checks",  '\0', 0, G_OPTION_ARG_NONE,     &async_flag,       "Enable asynchronous display detection", NULL},
      {"disable-async-ddc-checks",  '\0', 0, G_OPTION_ARG_NONE,     &async_flag,       "Disable asynchronous display detection", NULL},
      {"enable-async-i2c-bus-checks",  '\0', 0, G_OPTION_ARG_NONE, &async_check_i2c_flag, "Enable parallel examination of /dev/i2c devices (default)", NULL},
      {"disable-async-i2c-bus-checks", '\0', G_OPTION_FLAG_REVERSE,
                                          G_OPTION_ARG_NONE, &async_check_i2c_flag, "Disable parallel examination of /dev/i2c devices", NULL},
#endif
      {"async",   '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,     &async_flag,       "Enable asynchronous display detection (deprecated)", NULL},

      {"i2c-bus-checks-async-min",'\0', 0,
                                         G_OPTION_ARG_INT, &parsed_cmd->i2c_bus_check_async_min, i2c_bus_check_async_expl, NULL},
      {"ddc-checks-async-min",    '\0', G_OPTION_FLAG_NONE,
                                     G_OPTION_ARG_INT, &parsed_cmd->ddc_check_async_min, ddc_check_async_expl, NULL},

      {"skip-ddc-checks",'\0',0,G_OPTION_ARG_NONE,     &skip_ddc_checks_flag,     "Skip initial DDC checks",  NULL},

      {"lazy-sleep",  '\0', 0, G_OPTION_ARG_NONE, &deferred_sleep_flag, "Delay sleeps if possible",  NULL},
 //   {"defer-sleeps",'\0', 0, G_OPTION_ARG_NONE, &deferred_sleep_flag, "Delay sleeps if possible",  NULL},

      {"less-sleep" ,       '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &reduce_sleeps_specified, "Deprecated",  NULL},
      {"sleep-less" ,       '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &reduce_sleeps_specified, "Deprecated",  NULL},
      {"enable-sleep-less" ,'\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &reduce_sleeps_specified, "Deprecated",  NULL},
      {"disable-sleep-less",'\0', G_OPTION_FLAG_HIDDEN|G_OPTION_FLAG_REVERSE,
                                                         G_OPTION_ARG_NONE, &reduce_sleeps_specified, "Deprecated",  NULL},
      {"discard-caches",    '\0', G_OPTION_FLAG_OPTIONAL_ARG|G_OPTION_FLAG_HIDDEN,
                                  G_OPTION_ARG_CALLBACK, discard_cache_arg_func,    "Discard performance caches",  "cache type"},
      {"discard-cache",     '\0', G_OPTION_FLAG_OPTIONAL_ARG,
         G_OPTION_ARG_CALLBACK, discard_cache_arg_func,    "Discard performance caches",  "cache type"},

      // Behavior options
      {"maxtries",'\0', 0, G_OPTION_ARG_STRING,   &maxtrywork,       "Max try adjustment",  "comma separated list" },
      {"verify",  '\0', 0, G_OPTION_ARG_NONE,     &verify_flag,      "Read VCP value after setting it", NULL},
      {"noverify",'\0', 0, G_OPTION_ARG_NONE,     &noverify_flag,    "Do not read VCP value after setting it", NULL},

      {"mccs",    '\0', 0, G_OPTION_ARG_STRING,   &mccswork,         "Tailor feature handling to specific MCCS version",   "major.minor" },

      {"udf",     '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,     &enable_udf_flag,  enable_udf_expl,    NULL},
      {"enable-udf",'\0',0,G_OPTION_ARG_NONE,     &enable_udf_flag,  enable_udf_expl,    NULL},
      {"noudf",   '\0', G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_REVERSE,
                           G_OPTION_ARG_NONE,     &enable_udf_flag,  disable_udf_expl,   NULL},
      {"disable-udf",'\0', G_OPTION_FLAG_REVERSE,
                           G_OPTION_ARG_NONE,     &enable_udf_flag,  disable_udf_expl,   NULL},
      {"enable-heuristic-unsupported", '\0', G_OPTION_FLAG_HIDDEN,
         G_OPTION_ARG_NONE,     &enable_heuristic_unsupported_flag, "Perform heuristic unsupported feature detection", NULL},
      {"disable-heuristic-unsupported", '\0', G_OPTION_FLAG_REVERSE|G_OPTION_FLAG_HIDDEN,
         G_OPTION_ARG_NONE, &enable_heuristic_unsupported_flag, "Recognize only DDC/CI specified unsupported feature detection", NULL},
      {"enable-cross-instance-locks",
            '\0', 0, G_OPTION_ARG_NONE,     &enable_flock_flag,   enable_flock_expl,     NULL},
      {"disable-cross-instance-locks", '\0', G_OPTION_FLAG_REVERSE,
                     G_OPTION_ARG_NONE,     &enable_flock_flag,   disable_flock_expl ,   NULL},

      {"enable-try-get-edid-from-sysfs", '\0', 0,
                            G_OPTION_ARG_NONE,    &try_get_edid_from_sysfs,   enable_tgefs_expl, NULL},
      {"disable-try-get-edid-from-sysfs", '\0', G_OPTION_FLAG_REVERSE,
                           G_OPTION_ARG_NONE,     &try_get_edid_from_sysfs,   disable_tgefs_expl, NULL},
//      {"enable-watch-displays",  '\0', 0, G_OPTION_ARG_NONE, &watch_displays_flag, "Watch for display hotplug events", NULL },
//      {"disable-watch-displays", '\0', G_OPTION_FLAG_REVERSE,
//                                G_OPTION_ARG_NONE, &watch_displays_flag, "Do not watch for display hotplug events", NULL },

#ifdef ENABLE_USB
      {"enable-usb", '\0', G_OPTION_FLAG_NONE,
                               G_OPTION_ARG_NONE, &enable_usb_flag,  enable_usb_expl, NULL},
      {"disable-usb",'\0', G_OPTION_FLAG_REVERSE,
                               G_OPTION_ARG_NONE, &enable_usb_flag,  disable_usb_expl, NULL},
      {"nousb",      '\0', G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_REVERSE,
                               G_OPTION_ARG_NONE, &enable_usb_flag,  disable_usb_expl, NULL},
      {"ignore-usb-vid-pid", '\0', 0, G_OPTION_ARG_STRING_ARRAY, &ignored_vid_pid, "USB device to ignore","vid:pid" },
      {"ignore-hiddev", '\0', 0, G_OPTION_ARG_CALLBACK, ignored_hiddev_arg_func,  "USB device to ignore", "hiddev number"},
#endif

#ifdef FUTURE
      {"force-slave-address",
                  '\0', 0, G_OPTION_ARG_NONE,     &force_slave_flag, "Deprecated",         NULL},
#endif
      {"force-slave-address",
                  '\0', 0, G_OPTION_ARG_NONE,     &force_slave_flag, "Force I2C slave address",         NULL},

      {"use-file-io",
                  '\0', 0, G_OPTION_ARG_NONE,     &i2c_io_fileio_flag,"Use i2c-dev write()/read() calls by default",     NULL},
      {"use-ioctl-io",
                  '\0', G_OPTION_FLAG_HIDDEN,
                           G_OPTION_ARG_NONE,     &i2c_io_ioctl_flag, "Use i2c-dev ioctl() calls by default",     NULL},

      {"x52-no-fifo",'\0',0,G_OPTION_ARG_NONE,    &x52_no_fifo_flag, "Feature x52 does not have a FIFO queue", NULL},

      {"edid-read-size",
                      '\0', 0, G_OPTION_ARG_INT,  &edid_read_size_work, "Number of EDID bytes to read", "128,256" },
      {"i2c-source-addr",
                      '\0', 0, G_OPTION_ARG_STRING, &i2c_source_addr_work, "Alternative I2C source address", "source address"},

      {"force",   'f',  G_OPTION_FLAG_HIDDEN,
                           G_OPTION_ARG_NONE,     &force_flag,       "Deprecated",           NULL},
      {"permit-unknown-feature",
                     '\0', 0, G_OPTION_ARG_NONE,  &allow_unrecognized_feature_flag, "setvcp of unrecognized feature ok", NULL},
      {"timeout-i2c-io",'\0', G_OPTION_FLAG_HIDDEN,
                               G_OPTION_ARG_NONE, &timeout_i2c_io_flag, "Deprecated",  NULL},
//    {"no-timeout-ddc-io",'\0',G_OPTION_FLAG_REVERSE,
//                            G_OPTION_ARG_NONE,  &timeout_i2c_io_flag,   "Do not wrap DDC IO in timeout (default)",  NULL},
      {NULL},
   };

   GOptionEntry debug_options[] = {
      // Debugging
      {"excp",       '\0', G_OPTION_FLAG_HIDDEN,
                              G_OPTION_ARG_NONE,         &report_freed_excp_flag, "Report freed exceptions", NULL},
      {"trace",      '\0', 0, G_OPTION_ARG_STRING_ARRAY, &trace_classes,        "Trace classes",  "trace class name" },
//    {"trace",      '\0', 0, G_OPTION_ARG_STRING,       &tracework,            "Trace classes",  "comma separated list" },
      {"trcapi",     '\0', 0, G_OPTION_ARG_STRING_ARRAY, &parsed_cmd->traced_api_calls,      "Trace API call", "function name"},
      {"trcfunc",    '\0', 0, G_OPTION_ARG_STRING_ARRAY, &parsed_cmd->traced_functions,  "Trace functions","function name" },
      {"trcfrom",    '\0', 0, G_OPTION_ARG_STRING_ARRAY, &parsed_cmd->traced_calls,      "Trace call stack from function","function name" },
      {"trcfile",    '\0', 0, G_OPTION_ARG_STRING_ARRAY, &parsed_cmd->traced_files,      "Trace files",    "file name" },

      {"timestamp",  '\0', 0, G_OPTION_ARG_NONE,         &timestamp_trace_flag, "Prepend trace msgs with elapsed time",  NULL},
      {"ts",         '\0', 0, G_OPTION_ARG_NONE,         &timestamp_trace_flag, "Prepend trace msgs with elapsed time",  NULL},
      {"wall-timestamp",
                     '\0', 0, G_OPTION_ARG_NONE,         &wall_timestamp_trace_flag, "Prepend trace msgs with wall time",  NULL},
      {"wts",        '\0', 0, G_OPTION_ARG_NONE,         &wall_timestamp_trace_flag, "Prepend trace msgs with wall time",  NULL},
      {"thread-id",  '\0', 0, G_OPTION_ARG_NONE,         &thread_id_trace_flag, "Prepend trace msgs with thread id",  NULL},
      {"tid",        '\0', 0, G_OPTION_ARG_NONE,         &thread_id_trace_flag, "Prepend trace msgs with thread id",  NULL},
      {"process-id", '\0', 0, G_OPTION_ARG_NONE,         &process_id_trace_flag, "Prepend trace msgs with process id",  NULL},
      {"pid",        '\0', 0, G_OPTION_ARG_NONE,         &process_id_trace_flag, "Prepend trace msgs with process id",  NULL},
//    {"trace-to-file",'\0',0,G_OPTION_ARG_STRING,       &parsed_cmd->trace_destination,    "Send trace output here instead of terminal", "file name or \"syslog\""},
      {"trace-to-syslog-only",'\0', G_OPTION_FLAG_HIDDEN,
                              G_OPTION_ARG_NONE,         &trace_to_syslog_only_flag,  "Direct trace output only to syslog", NULL},
      {"libddcutil-trace-file",'\0', 0, G_OPTION_ARG_STRING,   &parsed_cmd->trace_destination,  "libddcutil trace file",  "file name"},
      {"stats-to-syslog",'\0', G_OPTION_FLAG_HIDDEN,
                              G_OPTION_ARG_NONE,         &stats_to_syslog_only_flag,  "Direct stats to syslog", NULL},

      {"debug-parse",'\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,        &debug_parse_flag,     "Report parsed command",    NULL},
      {"parse-only", '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,        &parse_only_flag,      "Terminate after parsing",  NULL},
      {"failsim",    '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_FILENAME,    &failsim_fn_work,      "Enable simulation", "control file name"},
      {"quickenv",   '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,        &quick_flag,           "Skip long running tests", NULL},
      {"enable-mock-data",
                     '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,        &mock_data_flag,       "Enable mock feature values", NULL},
      {"force-null-msg-for-unsupported",
                     '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,        &null_msg_for_unsupported_flag, "Simulate Null Msg indicates unsupported feature", NULL},

      // Generic options to aid development
      {"i1",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_STRING,   &i1_work,         "Special integer 1", "decimal or hex number" },
      {"i2",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_STRING,   &i2_work,         "Special integer 2", "decimal or hex number" },
      {"i3",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_STRING,   &i3_work,         "Special integer 3", "decimal or hex number" },
      {"i4",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_STRING,   &i4_work,         "Special integer 4", "decimal or hex number" },
      {"i5",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_STRING,   &i5_work,         "Special integer 5", "decimal or hex number" },
      {"i6",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_STRING,   &i6_work,         "Special integer 6", "decimal or hex number" },
      {"i7",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_STRING,   &i7_work,         "Special integer 7", "decimal or hex number" },
      {"i8",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_STRING,   &i8_work,         "Special integer 9", "decimal or hex number" },
      {"fl1",     '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_STRING,   &fl1_work,        "Special floating point number 1", "floating point number" },
      {"fl2",     '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_STRING,   &fl2_work,        "Special floating point number 2", "floating point number" },
      {"f1",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,     &f1_flag,         "Special flag 1",    NULL},
      {"f2",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,     &f2_flag,         "Special flag 2",    NULL},
      {"f3",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,     &f3_flag,         "Special flag 3",    NULL},
      {"f4",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,     &f4_flag,         "Special flag 4",    NULL},
      {"f5",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,     &f5_flag,         "Special flag 5",    NULL},
      {"f6",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,     &f6_flag,         "Special flag 6",    NULL},
      {"f7",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,     &f7_flag,         "Special flag 7",    NULL},
      {"f8",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,     &f8_flag,         "Special flag 8",    NULL},
      {"f9",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,     &f9_flag,         "Special flag 9",    NULL},
      {"f10",     '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,    &f10_flag,         "Special flag 10",   NULL},
      {"f11",     '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,    &f11_flag,         "Special flag 11",   NULL},
      {"f12",     '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,    &f12_flag,         "Special flag 12",   NULL},
      {"f13",     '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,    &f13_flag,         "Special flag 13",   NULL},
      {"f14",     '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_NONE,    &f14_flag,         "Special flag 14",   NULL},
      {"s1",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_STRING,   &parsed_cmd->s1,  "Special string 1",  "string"},
      {"s2",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_STRING,   &parsed_cmd->s2,  "Special string 2",  "string"},
      {"s3",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_STRING,   &parsed_cmd->s3,  "Special string 3",  "string"},
      {"s4",      '\0', G_OPTION_FLAG_HIDDEN,  G_OPTION_ARG_STRING,   &parsed_cmd->s4,  "Special string 4",  "string"},
      {NULL},
   };

   GOptionEntry final_options[] = {
      {G_OPTION_REMAINING,
                 '\0', 0,  G_OPTION_ARG_STRING_ARRAY, &cmd_and_args, "ARGUMENTS description",   "command [arguments...]"},
      { NULL }
   };

   Null_Terminated_String_Array temp_argv = ntsa_copy(argv, true);
   int hh_ndx = ntsa_find(temp_argv, "--hh");
   if (hh_ndx >= 0) {
      DBGMSG("--hh found");
      hidden_help_flag = true;
      free(temp_argv[hh_ndx]);
      temp_argv[hh_ndx] = g_strdup("-h");
   }

   if (hidden_help_flag) {
      unhide_options(ddcutil_only_options);
      unhide_options(common_options);
      unhide_options(debug_options);
   }

   bool preparse_verbose = ntsa_find(temp_argv, "--verbose") >= 0 ||
                           ntsa_find(temp_argv, "-v") >= 0;
   bool preparse_terse   = ntsa_find(temp_argv, "--terse") >= 0 ||
                           ntsa_find(temp_argv, "-t")      >= 0 ||
                           ntsa_find(temp_argv, "--brief") >= 0;

   GOptionGroup * all_options = g_option_group_new(
         "group name", "group description", "help description", NULL, NULL);
   if (parser_mode == MODE_DDCUTIL) {
      g_option_group_add_entries(all_options, ddcutil_only_options);
   }
   g_option_group_add_entries(all_options, common_options);
   g_option_group_add_entries(all_options, preparser_options);
#ifdef OLD
   if (parser_mode == MODE_LIBDDCUTIL) {
      g_option_group_add_entries(all_options, libddcutil_only_options);
   }
#endif
#ifndef FUTURE
   g_option_group_add_entries(all_options, debug_options);
#endif
   g_option_group_add_entries(all_options, final_options);

#ifdef FUTURE
   GOptionGroup * debug_option_group =
   g_option_group_new("debug","Debug Options", "(debug options description)",
                      NULL,   // user data
                      NULL);  // function to free user data
   g_option_group_add_entries(debug_option_group, debug_options);
   g_option_group_set_parse_hooks(debug_option_group, debug_pre_parse_func, debug_post_parse_func);
#endif


#ifdef WORKS
   // Preparser
   GError* preparser_error = NULL;
   GOptionContext* preparser_context = g_option_context_new("- Preparser");
   g_option_context_add_main_entries(preparser_context, preparser_options, NULL);
   // on --help, comes after usage line, before option detail
    g_option_context_set_summary(preparser_context, "Preparser help_summary");
    // on --help, comes at end after option detail
    g_option_context_set_description(preparser_context, "Preparser help description");
   g_option_context_set_help_enabled(preparser_context, false);
   Null_Terminated_String_Array temp_argv = ntsa_copy(argv, true);
   bool preparser_parsing_ok = g_option_context_parse_strv(preparser_context, &temp_argv, &preparser_error);
   if (!preparser_parsing_ok) {
      if (preparser_error)
         printf("%s\n" , preparser_error->message);
      else
         printf("Preparser option parsing failed");
   }
   ntsa_free(temp_argv, true);
   printf("(%s) hidden_help_flag: %s\n", __func__, sbool(hidden_help_flag));

   if (hidden_help_flag) {

   }
#endif


   // Main Parser

   GError* error = NULL;
   GOptionContext* context  = g_option_context_new("- DDC query and manipulation");
   // g_option_context_add_main_entries(context, option_entries, NULL);
   g_option_context_set_main_group(context, all_options);
#ifdef FUTURE
   g_option_context_add_group(context, debug_option_group);
#endif

   // comma delimited list of trace identifiers:
   // char * trace_group_string = strjoin(trace_group_names, trace_group_ct, ", ");
   // DBGMSG("traceGroupString = %s", traceGroupString);
   // const char * pieces[] = {tracing_option_help, "  Recognized trace classes: ", trace_group_string, "\n\n"};
   // tracing_option_help = strjoin(pieces, 4, NULL);

   // const char * pieces2[] = {command_argument_help, "  Recognized trace classes: ", trace_group_string, "\n\n"};
   // command_argument_help = strjoin(pieces, 4, NULL);

   // const char * pieces3[] = {commands_list_help, command_argument_help};
   // char * help_summary = strjoin(pieces3, 2, NULL);


   char * help_summary = NULL;
   if (preparse_verbose) {
      char * cmd_args_help = assemble_command_argument_help();
      const char * pieces[] = {commands_list_help, "\n", cmd_args_help};
      help_summary = strjoin(pieces, 3, NULL);
      free(cmd_args_help);
   }
   else {
      // const char * pieces[] = {commands_list_help, "\n"};
      // help_summary = strjoin(pieces, 2, NULL);
      help_summary = g_strdup(commands_list_help);
   }

   char * help_description = NULL;
   if (preparse_verbose) {
      const char * pieces[] = {monitor_selection_option_help,
                               tracing_multiple_call_option_help,
                               "\n",
                               trcfunc_multiple_call_option_help,
                               "\n",
                               trcfile_multiple_call_option_help,
                               "\n",
                               stats_multiple_call_option_help,
                               "\n",
                               maxtries_option_help,
                               NULL};
      help_description = strjoin(pieces, -1, NULL);
   }
   else if (preparse_terse) {
      help_description = NULL;
   }
   else {
      // const char * help_pieces[] = {monitor_selection_option_help};
      // help_description = strjoin(help_pieces, 1, NULL);
      // help_description = g_strdup(monitor_selection_option_help);
      help_description = g_strdup("For detailed help, use option \"--verbose\"");
                              //    "\nTo see all options, use option \"--hh\"");
   }

   // on --help, comes after usage line, before option detail
   g_option_context_set_summary(context, help_summary);
   free(help_summary);

   // on --help, comes at end after option detail
   g_option_context_set_description(context, help_description);
   free(help_description);

   g_option_context_set_help_enabled(context, true);

   /* From g_option_parse_documentation():
      If the parsing is successful, any parsed arguments are removed from the
      array and argc and argv are updated accordingly.

      From g_option_parse_strv() documentation:
      This function is similar to g_option_context_parse() except that it
      respects the normal memory rules when dealing with a strv instead of
      assuming that the passed-in array is the argv of the main function.
      In particular, strings that are removed from the arguments list will
      be freed using g_free().

      Pass a mangleable copy of argv to g_option_context_parse_strv().
   */
   // Null_Terminated_String_Array temp_argv = ntsa_copy(argv, true);
   bool parsing_ok = g_option_context_parse_strv(context, &temp_argv, &error);
   if (!parsing_ok) {
      char * mode_name = (parser_mode == MODE_DDCUTIL) ? "ddcutil" : "libddcutil";
      if (error) {
         // EMIT_PARSER_ERROR(errmsgs,  "%s option parsing failed: %s", mode_name, error->message);
         EMIT_PARSER_ERROR(errmsgs, "%s", error->message);
      }
      else
         EMIT_PARSER_ERROR(errmsgs,  "%s option parsing failed", mode_name);
   }
   ntsa_free(temp_argv, true);

   int rwo_flag_ct = 0;
   if (rw_only_flag)   rwo_flag_ct++;
   if (ro_only_flag)   rwo_flag_ct++;
   if (wo_only_flag)   rwo_flag_ct++;
   if (rwo_flag_ct > 1) {
      EMIT_PARSER_ERROR(errmsgs, "Options -rw-only, --ro-only, --wo-only are mutually exclusive");
      parsing_ok = false;
   }

   if (reduce_sleeps_specified)
      EMIT_PARSER_ERROR(errmsgs, "Deprecated option ignored: --enable-sleep-less, --disable-sleep-less, etc.");
   if (timeout_i2c_io_flag)
      EMIT_PARSER_ERROR(errmsgs, "Deprecated option ignored: --timeout-i2c-io");
   if (force_flag) {
      EMIT_PARSER_ERROR(errmsgs,"Deprecated option --force:  Use --permit-unknown-feature");
      allow_unrecognized_feature_flag = force_flag;
   }
   if (enable_cd_flag) {
      EMIT_PARSER_ERROR(errmsgs, "Warning: Experimental display information caching enabled");
   }
   if (async_flag) {
      EMIT_PARSER_ERROR(errmsgs, "Deprecated option ignored: --async.");
      EMIT_PARSER_ERROR(errmsgs, "Use --i2c-bus-checks-async-min (experimental) or --ddc-checks-async-min");
   }

#define LIBDDCUTIL_ONLY_OPTION(_name,_val) \
   do \
   if (_val) { \
      EMIT_PARSER_ERROR(errmsgs, "libddcutil only option: %s", _name); \
      parsing_ok = false; \
   } \
   while (0)

   if (parser_mode != MODE_LIBDDCUTIL) {
      LIBDDCUTIL_ONLY_OPTION("--trcapi",                parsed_cmd->traced_api_calls);
      LIBDDCUTIL_ONLY_OPTION("--profile-api",           profile_api_flag);
      LIBDDCUTIL_ONLY_OPTION("--libddcutil-trace-file", parsed_cmd->trace_destination);
      LIBDDCUTIL_ONLY_OPTION("--enable-watch-displays", watch_displays_flag);
   }

#undef LIBDDCUTIL_ONLY_OPTION

#define SET_CMDFLAG(_bit, _flag) \
   do { \
      if (_flag) \
         parsed_cmd->flags |= _bit; \
   } while(0)

#define SET_CLR_CMDFLAG(_bit, _flag) \
   do { \
      if (_flag) \
         parsed_cmd->flags |= _bit; \
      else \
         parsed_cmd->flags &= ~_bit; \
   } while(0)

#define SET_CLR_CMDFLAG2(_bit, _flag) \
   do { \
      if (_flag) \
         parsed_cmd->flags2 |= _bit; \
      else \
         parsed_cmd->flags2 &= ~_bit; \
   } while(0)

   parsed_cmd->output_level     = output_level;
   parsed_cmd->stats_types      = stats_work;
   parsed_cmd->ignored_hiddevs  = ignored_hiddev_work;
   SET_CMDFLAG(CMD_FLAG_VERBOSE_STATS,     verbose_stats);
   SET_CMDFLAG(CMD_FLAG_INTERNAL_STATS,    internal_stats);
   SET_CMDFLAG(CMD_FLAG_DDCDATA,           ddc_flag);
   SET_CMDFLAG(CMD_FLAG_FORCE_SLAVE_ADDR,  force_slave_flag);
   SET_CMDFLAG(CMD_FLAG_TIMESTAMP_TRACE,   timestamp_trace_flag);
   SET_CMDFLAG(CMD_FLAG_WALLTIME_TRACE,    wall_timestamp_trace_flag);
   SET_CMDFLAG(CMD_FLAG_THREAD_ID_TRACE,   thread_id_trace_flag);
   SET_CMDFLAG(CMD_FLAG_PROCESS_ID_TRACE,  process_id_trace_flag);
   SET_CMDFLAG(CMD_FLAG_VERIFY,            verify_flag || !noverify_flag);
   // if (verify_flag || !noverify_flag)
   //    parsed_cmd->flags |= CMD_FLAG_VERIFY;
#ifdef OLD
   SET_CMDFLAG(CMD_FLAG_NODETECT,          nodetect_flag);
   SET_CMDFLAG(CMD_FLAG_ASYNC_I2C_CHECK,   async_check_i2c_flag);
#endif
   SET_CMDFLAG(CMD_FLAG_REPORT_FREED_EXCP, report_freed_excp_flag);
   SET_CMDFLAG(CMD_FLAG_NOTABLE,           notable_flag);
   SET_CMDFLAG(CMD_FLAG_SHOW_UNSUPPORTED,  show_unsupported_flag);
   SET_CMDFLAG(CMD_FLAG_RW_ONLY,           rw_only_flag);
   SET_CMDFLAG(CMD_FLAG_RO_ONLY,           ro_only_flag);
   SET_CMDFLAG(CMD_FLAG_WO_ONLY,           wo_only_flag);
   SET_CMDFLAG(CMD_FLAG_FORCE_UNRECOGNIZED_VCP_CODE,  allow_unrecognized_feature_flag);
   SET_CLR_CMDFLAG(CMD_FLAG_ENABLE_UDF,    enable_udf_flag);
#ifdef ENABLE_USB
   SET_CMDFLAG(CMD_FLAG_ENABLE_USB,        enable_usb_flag);
#endif
#ifdef OLD
   SET_CMDFLAG(CMD_FLAG_TIMEOUT_I2C_IO,    timeout_i2c_io_flag);
   SET_CMDFLAG(CMD_FLAG_REDUCE_SLEEPS,     reduce_sleeps_flag);
#endif
   SET_CMDFLAG(CMD_FLAG_DSA2,              enable_dsa2_flag);
   SET_CMDFLAG(CMD_FLAG_DEFER_SLEEPS,      deferred_sleep_flag);
   SET_CMDFLAG(CMD_FLAG_F1,                f1_flag);
   SET_CMDFLAG(CMD_FLAG_F2,                f2_flag);
   SET_CMDFLAG(CMD_FLAG_F3,                f3_flag);
   SET_CMDFLAG(CMD_FLAG_F4,                f4_flag);
   SET_CMDFLAG(CMD_FLAG_F5,                f5_flag);
   SET_CMDFLAG(CMD_FLAG_F6,                f6_flag);
   SET_CMDFLAG(CMD_FLAG_F7,                f7_flag);
   SET_CMDFLAG(CMD_FLAG_F8,                f8_flag);
   SET_CMDFLAG(CMD_FLAG_F9,                f9_flag);
   SET_CMDFLAG(CMD_FLAG_F10,               f10_flag);
   SET_CMDFLAG(CMD_FLAG_F11,               f11_flag);
   SET_CMDFLAG(CMD_FLAG_F12,               f12_flag);
   SET_CMDFLAG(CMD_FLAG_F13,               f13_flag);
   SET_CMDFLAG(CMD_FLAG_F14,               f14_flag);
   SET_CMDFLAG(CMD_FLAG_WATCH_DISPLAY_HOTPLUG_EVENTS,    watch_displays_flag);
   SET_CMDFLAG(CMD_FLAG_X52_NO_FIFO,       x52_no_fifo_flag);
   SET_CMDFLAG(CMD_FLAG_SHOW_SETTINGS,     show_settings_flag);
   SET_CMDFLAG(CMD_FLAG_I2C_IO_FILEIO,     i2c_io_fileio_flag);
   SET_CMDFLAG(CMD_FLAG_I2C_IO_IOCTL,      i2c_io_ioctl_flag);
   SET_CMDFLAG(CMD_FLAG_QUICK,             quick_flag);
   SET_CMDFLAG(CMD_FLAG_MOCK,              mock_data_flag);
   SET_CMDFLAG(CMD_FLAG_PROFILE_API,       profile_api_flag);
   SET_CMDFLAG(CMD_FLAG_TRACE_TO_SYSLOG_ONLY, trace_to_syslog_only_flag);
   SET_CMDFLAG(CMD_FLAG_STATS_TO_SYSLOG, stats_to_syslog_only_flag);
   SET_CMDFLAG(CMD_FLAG_NULL_MSG_INDICATES_UNSUPPORTED_FEATURE, null_msg_for_unsupported_flag);
   SET_CMDFLAG(CMD_FLAG_HEURISTIC_UNSUPPORTED_FEATURES, enable_heuristic_unsupported_flag);
   SET_CMDFLAG(CMD_FLAG_SKIP_DDC_CHECKS,   skip_ddc_checks_flag);
   SET_CMDFLAG(CMD_FLAG_FLOCK,             enable_flock_flag);

   SET_CLR_CMDFLAG2(CMD_FLAG_TRY_GET_EDID_FROM_SYSFS,    try_get_edid_from_sysfs);
   SET_CLR_CMDFLAG(CMD_FLAG_ENABLE_CACHED_CAPABILITIES, enable_cc_flag);
// #ifdef REMOVED
   SET_CLR_CMDFLAG(CMD_FLAG_ENABLE_CACHED_DISPLAYS, enable_cd_flag);
// #endif

   if (discarded_caches_work) {
      parsed_cmd->discarded_cache_types = discarded_caches_work;
      SET_CMDFLAG(CMD_FLAG_DISCARD_CACHES, true);
   }

   if (failsim_fn_work) {
#ifdef ENABLE_FAILSIM
      // parsed_cmd->enable_failure_simulation = true;
      parsed_cmd->flags |= CMD_FLAG_ENABLE_FAILSIM;
      parsed_cmd->failsim_control_fn = failsim_fn_work;
#else
      EMIT_PARSER_ERROR(errmsgs, "ddcutil not built with failure simulation support.  --failsim option invalid.");
      parsing_ok = false;
#endif
   }

#undef SET_CMDFLAG
#undef SET_CLR_CMDFLAG

   // Create display identifier
   //
   // n. at this point parsed_cmd->pdid == NULL

   parsing_ok &= parse_display_identifier(
                    parsed_cmd,
                    errmsgs,
                    dispwork,
                    buswork,
                    hidwork,
                    usbwork,
                    edidwork,
                    mfg_id_work,
                    modelwork,
                    snwork);
   FREE(usbwork);
   FREE(edidwork);
   FREE(mfg_id_work);
   FREE(modelwork);
   FREE(snwork);

   if (maxtrywork) {
      parsing_ok &= parse_maxtrywork(maxtrywork, parsed_cmd, errmsgs);
      free(maxtrywork);
      maxtrywork = NULL;
   }

   if (mccswork) {
      parsing_ok &= parse_mccswork(mccswork, parsed_cmd, errmsgs);
      FREE(mccswork);
   }

   if (syslog_work) {
      DDCA_Syslog_Level level;
      bool this_ok = parse_syslog_level(syslog_work, &level, errmsgs);
      // printf("(%s) this_ok = %s\n", __func__, sbool(this_ok));
      if (this_ok)
         syslog_level = level;
      else
         parsing_ok = false;
      FREE(syslog_work);
   }
   parsed_cmd->syslog_level = syslog_level;

   if (i2c_source_addr_work) {
      int ival;
      bool ok = parse_int_work(i2c_source_addr_work, &ival, errmsgs);
      if (ival < 0 || ival > 255) {
         EMIT_PARSER_ERROR(errmsgs, "Source address must be a single byte value");
         ok = false;
      }
      if (ok) {
         parsed_cmd->flags = parsed_cmd->flags | CMD_FLAG_EXPLICIT_I2C_SOURCE_ADDR;
         parsed_cmd->explicit_i2c_source_addr = (uint8_t) ival;
      }
      parsing_ok &= ok;
      FREE(i2c_source_addr_work);
   }

#ifdef OLD
   if (i1_work) {
      bool ok = parse_int_work(i1_work, &parsed_cmd->i1, errmsgs);
      if (ok)
         parsed_cmd->flags2 = parsed_cmd->flags2 | CMD_FLAG2_I1_SET;
      parsing_ok &= ok;
      FREE(i1_work);
   }

   if (i2_work) {
      bool ok = parse_int_work(i2_work, &parsed_cmd->i2, errmsgs);
      if (ok)
         parsed_cmd->flags2 = parsed_cmd->flags2 | CMD_FLAG2_I2_SET;
      parsing_ok &= ok;
      FREE(i2_work);
   }

   if (i3_work) {
      bool ok = parse_int_work(i3_work, &parsed_cmd->i3, errmsgs);
      if (ok)
         parsed_cmd->flags2 = parsed_cmd->flags2 | CMD_FLAG2_I3_SET;
      parsing_ok &= ok;
      FREE(i2_work);
   }
#endif


#define SET_CMDFLAG_I(_n) \
      do { \
      if  (i ## _n ## _work) { \
         bool ok = parse_int_work(i ## _n ## _work, &parsed_cmd->i##_n, errmsgs); \
         if (ok) \
             parsed_cmd->flags2 = parsed_cmd->flags2 | CMD_FLAG2_I##_n##_SET; \
         parsing_ok &= ok; \
         free  (i ## _n ## _work); \
         i ## _n ## _work = NULL; \
      } \
   } while (0)

   SET_CMDFLAG_I(1);
   SET_CMDFLAG_I(2);
   SET_CMDFLAG_I(3);
   SET_CMDFLAG_I(4);
   SET_CMDFLAG_I(5);
   SET_CMDFLAG_I(6);
   SET_CMDFLAG_I(7);
   SET_CMDFLAG_I(8);

#undef SET_CMDFLAG_I

   if (fl1_work) {
     bool ok = str_to_float(fl1_work, &parsed_cmd->fl1);
     if (!ok)
        EMIT_PARSER_ERROR(errmsgs, "Invalid floating point number: %s", fl1_work);
     else
         parsed_cmd->flags = parsed_cmd->flags | CMD_FLAG_FL1_SET;
      parsing_ok &= ok;
      FREE(fl1_work);
   }

   if (fl2_work) {
     bool ok = str_to_float(fl2_work, &parsed_cmd->fl2);
     if (!ok)
        EMIT_PARSER_ERROR(errmsgs, "Invalid floating point number: %s", fl2_work);
     else
         parsed_cmd->flags = parsed_cmd->flags | CMD_FLAG_FL2_SET;
      parsing_ok &= ok;
      FREE(fl2_work);
   }

#ifdef ENABLE_USB
   if (ignored_vid_pid) {
      int ndx = 0;
      for (char * cur = ignored_vid_pid[ndx]; cur && ndx < 10; cur=ignored_vid_pid[++ndx]) {
         // DBGMSG("cur[%d]=%p -> %s", ndx, cur, cur);
         uint16_t vid;
         uint16_t pid;
         bool ok = parse_colon_separated_vid_pid(cur, &vid, &pid);
         if (!ok) {
            EMIT_PARSER_ERROR(errmsgs, "Invalid vid:pid value: %s", cur);
            parsing_ok = false;
         }
         else {
            // DBGMSG("vid = 0x%04x, pid=0x%04x", vid, pid);
            uint32_t ignored_vid_pid = vid << 16 | pid;
            // DBGMSG("ignored_vid_pid = 0x%08x", ignored_vid_pid);
            if (parsed_cmd->ignored_usb_vid_pid_ct < IGNORED_VID_PID_MAX)
               parsed_cmd->ignored_usb_vid_pids[parsed_cmd->ignored_usb_vid_pid_ct++] = ignored_vid_pid;
             else {
                EMIT_PARSER_ERROR(errmsgs, "Too many ignore-usb-vid-pid values");
                parsing_ok = false;
             }
         }
      }
      ntsa_free(ignored_vid_pid,true);
      ignored_vid_pid = NULL;
   }
#endif

   if (sleep_multiplier_work) {
      float multiplier = 0.0f;
      if (parse_sleep_multiplier(sleep_multiplier_work, &multiplier, errmsgs) ) {
         parsed_cmd->sleep_multiplier = multiplier;
         parsed_cmd->flags |= CMD_FLAG_EXPLICIT_SLEEP_MULTIPLIER;
      }
      else {
         parsing_ok = false;
      }
      FREE(sleep_multiplier_work);
   }

   if (min_dynamic_sleep_work) {
      parsing_ok &= parse_sleep_multiplier(min_dynamic_sleep_work, &parsed_cmd->min_dynamic_multiplier, errmsgs);
      FREE(min_dynamic_sleep_work);
   }


   DBGMSF(debug, "edid_read_size_work = %d", edid_read_size_work);
   if (edid_read_size_work !=  -1 &&
       edid_read_size_work != 128 &&
       edid_read_size_work !=   0 &&
       edid_read_size_work != 256)
   {
      EMIT_PARSER_ERROR(errmsgs, "Invalid EDID read size: %d", edid_read_size_work);
      parsing_ok = false;
   }
   else
      parsed_cmd->edid_read_size = edid_read_size_work;

   if (trace_classes) {
      parsing_ok &= parse_trace_classes(trace_classes, parsed_cmd, errmsgs);
      ntsa_free(trace_classes, true);
   }


   int rest_ct = 0;
   // don't pull debug into the if clause, need rest_ct to be set
   if (cmd_and_args) {
      for (; cmd_and_args[rest_ct] != NULL; rest_ct++) {
            DBGMSF(debug, "cmd_and_args[%d]: %s", rest_ct, cmd_and_args[rest_ct]);
      }
   }

   if (version_flag) {
      const char * version = get_full_ddcutil_version();
      const char * prefix = (output_level > DDCA_OL_TERSE) ? "ddcutil " : "";
      printf("%s%s\n", prefix, version);
      if (output_level >= DDCA_OL_VERBOSE)
         report_ddcutil_build_info();
      // if no command specified, include license in version information and terminate
      if (rest_ct == 0) {
         if (output_level > DDCA_OL_TERSE) {
            puts("Copyright (C) 2015-2024 Sanford Rockowitz");
            puts("License GPLv2: GNU GPL version 2 or later <http://gnu.org/licenses/gpl.html>");
            puts("This is free software: you are free to change and redistribute it.");
            puts("There is NO WARRANTY, to the extent permitted by law.");
         }
         exit(0);
      }
   }

   // All options processed.  Check for consistency, set defaults

   if (parser_mode == MODE_LIBDDCUTIL && rest_ct > 0) {
      EMIT_PARSER_ERROR(errmsgs, "Unrecognized: %s", cmd_and_args[0]);
         parsing_ok = false;
   }
   else if (parsing_ok && parser_mode == MODE_DDCUTIL && rest_ct == 0) {
      EMIT_PARSER_ERROR(errmsgs,  "No command specified");
      parsing_ok = false;
   }
   if (parsing_ok && parser_mode == MODE_DDCUTIL) {
      char * cmd = cmd_and_args[0];
      if (debug)
         printf("cmd=|%s|\n", cmd);
      Cmd_Desc * cmdInfo = find_command(cmd);
      if (cmdInfo == NULL) {
         EMIT_PARSER_ERROR(errmsgs,  "Unrecognized ddcutil command: %s", cmd);
         parsing_ok = false;
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
               EMIT_PARSER_ERROR(errmsgs,  "Too many arguments");
               parsing_ok = false;
               break;
            }
            parsed_cmd->args[argctr-1] = g_strdup(cmd_and_args[argctr]);
            argctr++;
         }
         parsed_cmd->argct = argctr-1;
         ntsa_free(cmd_and_args, true);

         // no more arguments specified
         if (argctr <= min_arg_ct) {
            EMIT_PARSER_ERROR(errmsgs,  "Missing argument(s)");
            parsing_ok = false;
         }

         if ( parsing_ok &&
              (parsed_cmd->cmd_id == CMDID_VCPINFO ||
               parsed_cmd->cmd_id == CMDID_GETVCP)
            )
         {
            parsed_cmd->fref = parse_feature_ids_or_subset(
                                  parsed_cmd->cmd_id,  parsed_cmd->args, parsed_cmd->argct);
            if (!parsed_cmd->fref) {
               parsing_ok = false;
               char * s = strjoin((const char **)parsed_cmd->args, parsed_cmd->argct, " ");
               EMIT_PARSER_ERROR(errmsgs,  "Invalid feature code(s) or subset: %s", s);
               free(s);
            }
         }

         // Ignore option --notable for vcpinfo
         if ( parsing_ok && parsed_cmd->cmd_id  == CMDID_VCPINFO) {
            parsed_cmd->flags &= ~CMD_FLAG_NOTABLE;
         }

         if (parsing_ok && parsed_cmd->cmd_id == CMDID_DISCARD_CACHE)
            parsing_ok &= parse_discard_args(parsed_cmd, errmsgs);

         if (parsing_ok && parsed_cmd->cmd_id == CMDID_GETVCP
                        && (parsed_cmd->flags & CMD_FLAG_WO_ONLY) ) {
            fprintf(stdout, "Ignoring option --wo-only\n");
            parsed_cmd->flags &= ~CMD_FLAG_WO_ONLY;
         }

         if (parsing_ok && parsed_cmd->cmd_id == CMDID_SETVCP)
            parsing_ok &= parse_setvcp_args(parsed_cmd,errmsgs);

         if (parsing_ok && parsed_cmd->pdid) {
            if (!cmdInfo->supported_options & Option_Explicit_Display) {
               EMIT_PARSER_ERROR(errmsgs,  "%s does not support explicit display option\n", cmdInfo->cmd_name);
               parsing_ok = false;
            }
         }

#ifdef OUT
         if (parsing_ok && !(parsed_cmd->cmd_id == CMDID_GETVCP || parsed_cmd->cmd_id == CMDID_SETVCP)) {
            if (skip_ddc_checks_flag) {
               EMIT_PARSER_ERROR(errmsgs, "Option --skip-ddc-checks valid only for getvcp or setvcp");
               parsing_ok = false;
            }
         }
         if (parsing_ok && skip_ddc_checks_flag && buswork < 0) {
            EMIT_PARSER_ERROR(errmsgs, "Option --skip-ddc-checks valid only with option --bus");
            parsing_ok = false;
         }
#endif
      }  // recognized command
   }

   parsing_ok &= validate_output_level(parsed_cmd);

   DBGMSF(debug, "Calling g_option_context_free(), context=%p...", context);
   g_option_context_free(context);

   if (debug || debug_parse_flag) {
      DBGMSG("parsing_ok=%s", sbool(parsing_ok));
      dbgrpt_parsed_cmd(parsed_cmd, 0);
   }

   if (!parsing_ok) {
      free_parsed_cmd(parsed_cmd);
      parsed_cmd = NULL;
   }
   if (debug) {
      DBGMSG("Before return: argc=%d", argc);
      int ndx = 0;
      for (; ndx < argc; ndx++) {
         DBGMSG("argv[%d] = |%s|", ndx, argv[ndx]);
      }
   }
   if (parse_only_flag && parsing_ok) {
      free_parsed_cmd(parsed_cmd);
      parsed_cmd = NULL;
   }
   DBGMSF(debug, "Returning: %p", parsed_cmd);
   return parsed_cmd;
}
