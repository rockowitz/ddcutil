/** @file api_base.c
 *
 *  C API base functions.
 */

// Copyright (C) 2015-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#define _GNU_SOURCE 1
#include <assert.h>
#include <dlfcn.h>     // _GNU_SOURCE for dladdr()
#include <errno.h>
#include <glib-2.0/glib.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>

#include "public/ddcutil_c_api.h"

#include "util/ddcutil_config_file.h"
#include "util/debug_util.h"
#include "util/file_util.h"
#include "util/report_util.h"
#include "util/sysfs_filter_functions.h"
#include "util/xdg_util.h"

#include "base/base_services.h"
#include "base/build_info.h"
#include "base/core_per_thread_settings.h"
#include "base/core.h"
#include "base/dsa2.h"
#include "base/parms.h"
#include "base/per_display_data.h"
#include "base/per_thread_data.h"
#include "base/rtti.h"
#include "base/trace_control.h"
#include "base/tuned_sleep.h"

#include "cmdline/cmd_parser.h"
#include "cmdline/parsed_cmd.h"

#include "i2c/i2c_bus_core.h"   // for testing watch_devices
#include "i2c/i2c_display_lock.h"
#include "i2c/i2c_execute.h"    // for i2c_set_addr()

#include "ddc/ddc_common_init.h"
#include "ddc/ddc_displays.h"
#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_serialize.h"
#include "ddc/ddc_services.h"
#include "ddc/ddc_try_data.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_watch_displays.h"

#include "libmain/api_error_info_internal.h"
#include "libmain/api_base_internal.h"
#include "libmain/api_services_internal.h"

//
// Forward Declarations
//

void init_api_base();

//
// Globals
//

bool library_initialized = false;
bool library_initialization_failed = false;
static bool client_opened_syslog = false;
static bool enable_init_msgs = false;
static FILE * flog = NULL;
static DDCA_Stats_Type requested_stats = 0;
static bool per_display_stats = false;
static bool dsa_detail_stats;


//
// Precondition Failure
//

DDCI_Api_Precondition_Failure_Mode api_failure_mode = DDCI_PRECOND_STDERR_RETURN;

#ifdef UNUSED
static DDCI_Api_Precondition_Failure_Mode
ddci_set_precondition_failure_mode(
      DDCI_Api_Precondition_Failure_Mode failure_mode)
{
   DDCI_Api_Precondition_Failure_Mode old = api_failure_mode;
   api_failure_mode = failure_mode;
   return old;
}

static DDCI_Api_Precondition_Failure_Mode
ddci_get_precondition_failure_mode()
{
   return api_failure_mode;
}
#endif


//
// Library Build Information
//

DDCA_Ddcutil_Version_Spec
ddca_ddcutil_version(void) {
   static DDCA_Ddcutil_Version_Spec vspec = {255,255,255};
   static bool vspec_init = false;

   if (!vspec_init) {
#ifndef NDEBUG
      int ct =
#endif
            sscanf(get_base_ddcutil_version(),
                      "%hhu.%hhu.%hhu", &vspec.major, &vspec.minor, &vspec.micro);
#ifndef NDEBUG
      assert(ct == 3);
#endif
      vspec_init = true;
   }
   // DBGMSG("Returning: %d.%d.%d", vspec.major, vspec.minor, vspec.micro);
   return vspec;
}


/**  Returns the ddcutil version as a string in the form "major.minor.micro".
 *
 */
const char *
ddca_ddcutil_version_string(void) {
   return get_base_ddcutil_version();
}


// Returns the full ddcutil version as a string that may be suffixed with an extension
const char *
ddca_ddcutil_extended_version_string(void) {
   return get_full_ddcutil_version();
}


#ifdef UNUSED
// Indicates whether the ddcutil library was built with support for USB connected monitors.
bool
ddca_built_with_usb(void) {
#ifdef ENABLE_USB
   return true;
#else
   return false;
#endif
}
#endif

// Alternative to individual ddca_built_with...() functions.
// conciseness vs documentability
// how to document bits?   should doxygen doc be in header instead?

DDCA_Build_Option_Flags
ddca_build_options(void) {
   uint8_t result = 0x00;
#ifdef ENABLE_USB
         result |= DDCA_BUILT_WITH_USB;
#endif
#ifdef FAILSIM_ENABLED
         result |= DDCA_BUILT_WITH_FAILSIM;
#endif
   // DBGMSG("Returning 0x%02x", result);
   return result;
}


const char *
ddca_libddcutil_filename(void) {
   Dl_info info = {NULL,NULL,NULL,NULL};
   static char fullname[PATH_MAX];
   static char * p = NULL;
   if (!p) {
      dladdr(ddca_build_options, &info);
      p = realpath(info.dli_fname, fullname);
      assert(p == fullname);
   }
   return p;
}


Error_Info* perform_parse(
      int     new_argc,
      char ** new_argv,
      char *  combined,
      Parsed_Cmd ** parsed_cmd_loc)
{
   GPtrArray * errmsgs = g_ptr_array_new_with_free_func(g_free);
   bool debug = false;

   Error_Info * result = NULL;
   DBGF(debug, "Calling parse_command(), errmsgs=%p\n", errmsgs);
   *parsed_cmd_loc = parse_command(new_argc, new_argv, MODE_LIBDDCUTIL, errmsgs);
   DBGF(debug, "*parsed_cmd_loc=%p, errmsgs->len=%d", *parsed_cmd_loc, errmsgs->len);
   ASSERT_IFF(*parsed_cmd_loc, errmsgs->len == 0);
   if (!*parsed_cmd_loc) {
      if (test_emit_syslog(DDCA_SYSLOG_ERROR)) {
         syslog(LOG_ERR, "Invalid option string: %s",  combined);
         for (int ndx = 0; ndx < errmsgs->len; ndx++) {
             char * msg =  g_ptr_array_index(errmsgs,ndx);
             syslog(LOG_ERR, "%s", msg);
         }
      }
      result = ERRINFO_NEW(DDCRC_INVALID_CONFIG_FILE, "Invalid option string: %s",  combined);
      for (int ndx = 0; ndx < errmsgs->len; ndx++) {
         char * msg =  g_ptr_array_index(errmsgs, ndx);
         errinfo_add_cause(result, errinfo_new(DDCRC_INVALID_CONFIG_FILE, __func__, msg));
      }
   }
   else {
      if (debug)
         dbgrpt_parsed_cmd(*parsed_cmd_loc, 1);
   }
   g_ptr_array_free(errmsgs, true);
   ASSERT_IFF(*parsed_cmd_loc, !result);
   return result;

}


static inline void emit_parse_info_msg(const char * msg, GPtrArray* infomsgs) {
   if (infomsgs)
      g_ptr_array_add(infomsgs, g_strdup_printf("%s%s", "libddcutil: ", msg));
   SYSLOG2(DDCA_SYSLOG_NOTICE,"%s", msg);
}


//
// Initialization
//
static Error_Info *
get_parsed_libmain_config(const char * libopts_string,
                          bool         disable_config_file,
                          GPtrArray*   infomsgs,
                          Parsed_Cmd** parsed_cmd_loc)
{
   bool debug = false;
   DBGF(debug, "Starting. disable_config_file = %s, libopts_string = %sn",
               sbool(disable_config_file), libopts_string);

   char * msg = g_strdup_printf("Options passed from client: %s",
                                (libopts_string) ? libopts_string : "");
   emit_parse_info_msg(msg, infomsgs);
   free(msg);

   Error_Info * result = NULL;
   *parsed_cmd_loc = NULL;

   char ** libopts_tokens = NULL;
   int libopts_token_ct = 0;
   if (libopts_string) {
      libopts_token_ct = tokenize_options_line(libopts_string, &libopts_tokens);
      DBGF(debug, "libopts_token_ct = %d, libopts_tokens=%p:", libopts_token_ct,libopts_tokens);
      if (debug)
         ntsa_show(libopts_tokens);
   }
   Null_Terminated_String_Array cmd_name_array = calloc(2 + libopts_token_ct, sizeof(char*));
   cmd_name_array[0] = strdup("libddcutil");   // so libddcutil not a special case for parser
   int ndx = 0;
   for (; ndx < libopts_token_ct; ndx++)
      cmd_name_array[ndx+1] = g_strdup(libopts_tokens[ndx]);
   cmd_name_array[ndx+1] = NULL;
   ntsa_free(libopts_tokens,true);

   DBGF(debug, "cmd_name_array=%p, cmd_name_array[1]=%p -> %s",
                cmd_name_array, cmd_name_array[0], cmd_name_array[0]);

   char ** new_argv = NULL;
   int     new_argc = 0;
   char *  untokenized_option_string = NULL;

   if (disable_config_file) {
      DBGF(debug, "config file disabled");
      new_argv = ntsa_copy(cmd_name_array, true);
      new_argc = ntsa_length(cmd_name_array);
      ntsa_free(cmd_name_array, true);
   }
   else {
      GPtrArray * errmsgs = g_ptr_array_new_with_free_func(g_free);
      char *  config_fn = NULL;
      DBGF(debug, "Calling apply_config_file()...");
      int apply_config_rc = apply_config_file(
                                    "libddcutil",  // use this section of config file
                                    ntsa_length(cmd_name_array), cmd_name_array,
                                    &new_argc,
                                    &new_argv,
                                    &untokenized_option_string,
                                    &config_fn,
                                    errmsgs);
      ntsa_free(cmd_name_array, true);
      assert(apply_config_rc <= 0);
      ASSERT_IFF(apply_config_rc == 0, errmsgs->len == 0);
      // DBGF(debug, "Calling ntsa_free(cmd_name_array=%p", cmd_name_array);

      DBGF(debug, "apply_config_file() returned: %d (%s), new_argc=%d, new_argv=%p:",
                  apply_config_rc, psc_desc(apply_config_rc), new_argc, new_argv);

      if (apply_config_rc == -EBADMSG) {
         result = errinfo_new(DDCRC_INVALID_CONFIG_FILE, __func__,
                              "Error(s) processing configuration file: %s", config_fn);
         for (int ndx = 0; ndx < errmsgs->len; ndx++) {
            errinfo_add_cause(result,
                  errinfo_new(DDCRC_INVALID_CONFIG_FILE, __func__, g_ptr_array_index(errmsgs, ndx)));
         }

      }
      // else if (apply_config_rc == -ENOENT) {
      //    result = errinfo_new(-ENOENT, __func__, "Configuration file not found");
      // }
      else if (apply_config_rc < 0) {
         result = errinfo_new(apply_config_rc, __func__,
                     "Unexpected error reading configuration file: %s", psc_desc(apply_config_rc));
      }
      else {
         assert( new_argc == ntsa_length(new_argv) );
         if (debug)
            ntsa_show(new_argv);

         if (untokenized_option_string && strlen(untokenized_option_string) > 0) {
            char * msg = g_strdup_printf("Using options from %s: %s",
                                         config_fn, untokenized_option_string);
            emit_parse_info_msg(msg, infomsgs);
            free(msg);
         }
      }
      g_ptr_array_free(errmsgs, true);
      free(config_fn);
   }

   if (!result) {   // if no errors
      assert(new_argc >= 1);
      char * combined = strjoin((const char**)(new_argv+1), new_argc, " ");
      char * msg = g_strdup_printf("Applying combined options: %s", combined);
      emit_parse_info_msg(msg, infomsgs);
      free(msg);

      result = perform_parse(new_argc, new_argv, combined, parsed_cmd_loc);
      ntsa_free(new_argv, true);
      free(combined);
      free(untokenized_option_string);
   }

   DBGF(debug, "Done.     *parsed_cmd_loc=%p. Returning %s",
              *parsed_cmd_loc, errinfo_summary(result));

   ASSERT_IFF(*parsed_cmd_loc, !result);
   return result;
}


#ifdef TESTING_CLEANUP
void done() {
   printf("(%s) Starting\n", __func__);
   _ddca_terminate();
   SYSLOG(LOG_INFO, "(%s) executing done()", __func__);
   printf("(%s) Done.\n", __func__);
}

void dummy_sigterm_handler() {
   printf("(%s) Executing. library_initialized = %s\n",
         __func__, SBOOL(library_initialized));
}

void atexit_func() {
   printf("(%s) Executing. library_initalized = %s\n",
         __func__, SBOOL(library_initialized));
}
#endif



/** Initializes the ddcutil library module.
 *
 *  Called automatically when the shared library is loaded.
 *
 *  Registers functions in RTTI table, performs additional initialization
 *  that cannot fail.
 */
void  __attribute__ ((constructor))
_ddca_new_init(void) {
   bool debug = false;
   char * s = getenv("DDCUTIL_DEBUG_LIBINIT");
   if (s && strlen(s) > 0)
      debug = true;

   DBGF(debug, "Starting. library_initialized=%s", sbool(library_initialized));

   init_api_base();         // registers functions in RTTI table
   init_base_services();    // initializes tracing related modules
   init_ddc_services();     // initializes i2c, usb, ddc, vcp, dynvcp
   init_api_services();     // other files in directory libmain

#ifdef TESTING_CLEANUP
   // int atexit_rc = atexit(done);   // TESTING CLEANUP
   // printf("(%s) atexit() returned %d\n", __func__, atexit_rc);
#endif

   DBGF(debug, "Done.");
}


//
// Profiling
//

void profiling_enable(bool enabled) {
   ptd_api_profiling_enabled = enabled;
}

void profiling_reset() {
   ptd_profile_reset_all_stats();
}

void profile_start_call(void * func) {
   ptd_profile_function_start(func);
}

void profile_end_call(void * func) {
   ptd_profile_function_end(func);
}

void profile_report(FILE * dest, bool by_thread) {
   if (dest) {
      rpt_push_output_dest(dest);
   }
   ptd_profile_report_all_threads(0);
   ptd_profile_report_stats_summary(0);
   if (dest) {
      rpt_pop_output_dest();
   }
}


//
// Tracing
//

void
init_library_trace_file(char * library_trace_file, bool enable_syslog, bool debug) {
   DBGF(debug, "library_trace_file = \"%s\", enable_syslog = %s", library_trace_file, sbool(enable_syslog));
   char * trace_file = (library_trace_file[0] != '/')
          ? xdg_state_home_file("ddcutil", library_trace_file)
          : g_strdup(library_trace_file);
   DBGF(debug, "Setting trace destination %s", trace_file);
   SYSLOG2(DDCA_SYSLOG_NOTICE, "Trace destination: %s", trace_file);

   fopen_mkdir(trace_file, "a", stderr, &flog);
   if (flog) {
      time_t trace_start_time = time(NULL);
      char * trace_start_time_s = asctime(localtime(&trace_start_time));
      if (trace_start_time_s[strlen(trace_start_time_s)-1] == 0x0a)
           trace_start_time_s[strlen(trace_start_time_s)-1] = 0;
      fprintf(flog, "%s tracing started %s\n", "libddcutil", trace_start_time_s);
      DBGF(debug, "Writing %s trace output to %s", "libddcutil",trace_file);
      set_default_thread_output_settings(flog, flog);
      set_fout(flog);
      set_ferr(flog);

      rpt_set_default_output_dest(flog);    // for future threads
      rpt_push_output_dest(flog);           // for this thread
   }
   else {
      fprintf(stderr, "Error opening libddcutil trace file %s: %s\n",
                      trace_file, strerror(errno));
      SYSLOG2(DDCA_SYSLOG_ERROR, "Error opening libddcutil trace file %s: %s",
                             trace_file, strerror(errno));
   }
   free(trace_file);
   DBGF(debug, "Done.");
}


/** Cleanup at library termination
 *
 *  - Terminates thread that watches for display addition or removal.
 *  - Releases heap memory to avoid error reports from memory analyzers.
 */
void __attribute__ ((destructor))
_ddca_terminate(void) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API, "library_initialized = %s", SBOOL(library_initialized));
   if (library_initialized) {
      if (debug)
         dbgrpt_display_locks(2);
      if (dsa2_is_enabled())
         dsa2_save_persistent_stats();
      if (display_caching_enabled)
         ddc_store_displays_cache();
      ddc_discard_detected_displays();
      if (requested_stats)
         ddc_report_stats_main(requested_stats, per_display_stats, dsa_detail_stats, false, 0);
      DDCA_Display_Event_Class active_classes;
      ddc_stop_watch_displays(/*wait=*/ false, &active_classes);   // in case it was started
      terminate_ddc_services();
      terminate_base_services();
      free_regex_hash_table();
      library_initialized = false;
      if (flog)
         fclose(flog);
      DBGTRC_DONE(debug, DDCA_TRC_API, "library termination complete");
   }
   else {
      DBGTRC_DONE(debug, DDCA_TRC_API, "library was already terminated");   // should be impossible
   }
   // special handling for termination msg
   if (syslog_level > DDCA_SYSLOG_NEVER)
      syslog(LOG_NOTICE, "libddcutil terminating.");
   if (syslog_level > DDCA_SYSLOG_NEVER && !client_opened_syslog)
      closelog();
}


Error_Info *
set_master_errinfo_from_init_errors(
      GPtrArray * errs) // array of Error_Info *
{
   bool debug = false;
   DBGF(debug, "Starting. errs=%p", errs);
   Error_Info * master_error = NULL;
   if (errs && errs->len > 0) {
      master_error = errinfo_new(DDCRC_BAD_DATA, __func__, "Invalid configuration options");
      for (int ndx = 0; ndx < errs->len; ndx++) {
         Error_Info * cur = g_ptr_array_index(errs, ndx);
         errinfo_add_cause(master_error, cur);
      }
      g_ptr_array_free(errs, false);
   }
   DBGF(debug, "Done.  Returning %p");
   return master_error;
}


DDCA_Status
set_ddca_error_detail_from_init_errors(
      GPtrArray * errs) // array of Error_Info *
{
   bool debug = false;
   DDCA_Status ddcrc = 0;
   if (errs && errs->len > 0) {
      Error_Info * master_error = errinfo_new(DDCRC_BAD_DATA, __func__, "Invalid configuration options");
      ddcrc = DDCRC_BAD_DATA;
      for (int ndx = 0; ndx < errs->len; ndx++) {
         Error_Info * cur = g_ptr_array_index(errs, ndx);
         errinfo_add_cause(master_error, cur);
      }
      DDCA_Error_Detail * public_error_detail = error_info_to_ddca_detail(master_error);
      errinfo_free_with_report(master_error, debug, __func__);
      save_thread_error_detail(public_error_detail);
   }
   // clear if no errors?
   return ddcrc;
}


DDCA_Syslog_Level ddca_syslog_level_from_name(const char * name) {
   return syslog_level_name_to_value(name);
}


void report_parse_errors0(Error_Info * erec, int depth, int max_depth) {
   char * edesc = psc_text(erec->status_code);

   if (depth == 0)  {
      rpt_vstring(depth, "%s: %s", edesc, erec->detail);
   }
   else {
      rpt_vstring(depth, "%s", erec->detail);
   }
   if (depth < max_depth) {
      if (erec->cause_ct > 0) {
         for (int ndx = 0; ndx < erec->cause_ct; ndx++) {
            Error_Info * cur = erec->causes[ndx];
            report_parse_errors0(cur, depth+1, max_depth);
         }
      }
   }
}


void report_parse_errors(Error_Info * erec) {
   if (erec) {
      rpt_push_output_dest(ferr());
      report_parse_errors0(erec, 0, 3);
      rpt_pop_output_dest();
   }
}


DDCA_Status
ddci_init(const char *      libopts,
          DDCA_Syslog_Level syslog_level_arg,
          DDCA_Init_Options opts,
          char***           infomsg_loc)
{
   bool debug = false;
   char * s = getenv("DDCUTIL_DEBUG_LIBINIT");
   if (s && strlen(s) > 0)
      debug = true;

   DBGF(debug, "Starting. library_initialized=%s", sbool(library_initialized));

   if (infomsg_loc)
      *infomsg_loc = NULL;

   Parsed_Cmd * parsed_cmd = NULL;
   Error_Info * master_error = NULL;
   if (library_initialized) {
      master_error = ERRINFO_NEW(DDCRC_INVALID_OPERATION, "libddcutil already initialized");
      SYSLOG2(DDCA_SYSLOG_ERROR, "libddcutil already initialized");
   }
   else {
      enable_init_msgs = opts & DDCA_INIT_OPTIONS_ENABLE_INIT_MSGS;
      // enable_init_msgs = true;  // *** TEMP ***
      client_opened_syslog = opts & DDCA_INIT_OPTIONS_CLIENT_OPENED_SYSLOG;
      if (syslog_level_arg == DDCA_SYSLOG_NOT_SET)
         syslog_level_arg = DEFAULT_LIBDDCUTIL_SYSLOG_LEVEL;
      if (syslog_level_arg != DDCA_SYSLOG_NEVER) {
         enable_syslog = true;
         if (!client_opened_syslog) {
         openlog("libddcutil",       // prepended to every log message
                 LOG_CONS | LOG_PID, // write to system console if error sending to system logger
                                     // include caller's process id
                 LOG_USER);          // generic user program, syslogger can use to determine how to handle
         }
         // special handling for start and termination msgs
         // always output if syslog is opened
         syslog(LOG_NOTICE, "Initializing libddcutil.  ddcutil version: %s, shared library: %s",
                   get_full_ddcutil_version(), ddca_libddcutil_filename());
      }
      syslog_level = syslog_level_arg;  // global in trace_control.h

      GPtrArray* infomsgs = NULL;
         infomsgs = g_ptr_array_new_with_free_func(g_free);

      if ((opts & DDCA_INIT_OPTIONS_DISABLE_CONFIG_FILE) && !libopts) {
         parsed_cmd = new_parsed_cmd();
      }
      else {
         master_error = get_parsed_libmain_config(
                           libopts,
                           opts & DDCA_INIT_OPTIONS_DISABLE_CONFIG_FILE,
                           infomsgs,
                           &parsed_cmd);
         ASSERT_IFF(master_error, !parsed_cmd);

         if (enable_init_msgs && infomsgs && infomsgs->len > 0) {
            for (int ndx = 0; ndx < infomsgs->len; ndx++)
               fprintf(fout(), "%s\n", (char*) g_ptr_array_index(infomsgs, ndx));
         }
         if (infomsg_loc) {
            *infomsg_loc = g_ptr_array_to_ntsa(infomsgs, /*duplicate=*/true);
         }
         g_ptr_array_free(infomsgs, true);
      }
      if (!master_error) {
         if (parsed_cmd->trace_destination) {
            DBGF(debug, "Setting library trace file: %s", parsed_cmd->trace_destination);
            init_library_trace_file(parsed_cmd->trace_destination, enable_syslog, debug);
         }
         master_error = init_tracing(parsed_cmd);
      }
      if (!master_error) {
         requested_stats = parsed_cmd->stats_types;
         ptd_api_profiling_enabled = parsed_cmd->flags & CMD_FLAG_PROFILE_API;
         per_display_stats = parsed_cmd->flags & CMD_FLAG_VERBOSE_STATS;
         dsa_detail_stats = parsed_cmd->flags & CMD_FLAG_INTERNAL_STATS;
         if (!submaster_initializer(parsed_cmd))
            master_error = ERRINFO_NEW(DDCRC_UNINITIALIZED, "Initialization failed");
      }
   }

   assert(master_error || parsed_cmd);  // avoid null-dereference warning
   DDCA_Status ddcrc = 0;
   if (master_error) {
      ddcrc = master_error->status_code;
      DDCA_Error_Detail * public_error_detail = error_info_to_ddca_detail(master_error);
      save_thread_error_detail(public_error_detail);
      if (test_emit_syslog(DDCA_SYSLOG_ERROR)) {
         SYSLOG2(DDCA_SYSLOG_ERROR, "Library initialization failed: %s", psc_desc(master_error->status_code));
         for (int ndx = 0; ndx < master_error->cause_ct; ndx++) {
            SYSLOG2(DDCA_SYSLOG_ERROR, "%s", master_error->causes[ndx]->detail);
         }
      }
      if (enable_init_msgs) {
         printf("(%s) calling report_parse_errors()\n", __func__);
         report_parse_errors(master_error);
      }
      errinfo_free(master_error);
      library_initialization_failed = true;
   }
   else {
      i2c_detect_buses();
      ddc_ensure_displays_detected();
#ifdef OUT
      if (parsed_cmd->flags&CMD_FLAG_WATCH_DISPLAY_HOTPLUG_EVENTS) {
         ddc_start_watch_displays(DDCA_EVENT_CLASS_DISPLAY_CONNECTION | DDCA_EVENT_CLASS_DPMS);
         SYSLOG2(DDCA_SYSLOG_NOTICE,
               "Started watch displays for DDCA_EVENT_CLASS_DISPLAY_CONNECTION | DDCA_EVENT_CLASS_DPMS");
      }
#endif
      library_initialized = true;
      library_initialization_failed = false;
      SYSLOG2(DDCA_SYSLOG_NOTICE, "Library initialization complete.");
   }
   free_parsed_cmd(parsed_cmd);

   DBGF(debug, "Done.    Returning: %s", psc_desc(ddcrc));

   return ddcrc;
}


DDCA_Status
ddca_init(const char *      libopts,
          DDCA_Syslog_Level syslog_level_arg,
          DDCA_Init_Options opts)
{
   return ddci_init(libopts, syslog_level_arg, opts, NULL);
}

DDCA_Status
ddca_init2(const char *     libopts,
          DDCA_Syslog_Level syslog_level_arg,
          DDCA_Init_Options opts,
          char***           infomsg_loc
          )
{
   return ddci_init(libopts, syslog_level_arg, opts, infomsg_loc);
}


DDCA_Status
ddca_start_watch_displays(DDCA_Display_Event_Class enabled_classes) {
   bool debug = false;
   API_PROLOG(debug, "Starting");

   DDCA_Error_Detail * edet = NULL;
   if (!drm_enabled) {
      edet = new_ddca_error_detail(DDCRC_INVALID_OPERATION,
               "Display hotplug detection requires DRM enabled video drivers");
   }
   else {
      Error_Info * erec = ddc_start_watch_displays(enabled_classes);
      edet = error_info_to_ddca_detail(erec);
      ERRINFO_FREE(erec);
   }

   DDCA_Status ddcrc = 0;
   if (edet) {
      ddcrc = edet->status_code;
      save_thread_error_detail(edet);
   }
   API_EPILOG(debug, ddcrc, "");
}


DDCA_Status
ddca_stop_watch_displays(bool wait) {
   bool debug = false;
   API_PROLOG(debug, "Starting");
   DDCA_Display_Event_Class active_classes;
   DDCA_Status ddcrc = ddc_stop_watch_displays(wait, &active_classes);
   API_EPILOG(debug, ddcrc, "");
}


DDCA_Status
ddca_get_active_watch_classes(DDCA_Display_Event_Class * classes_loc) {
   bool debug = false;
   API_PROLOG(debug, "Starting classes_loc=%p", classes_loc);
   DDCA_Status ddcrc = ddc_get_active_watch_classes(classes_loc);
   API_EPILOG(debug, ddcrc, "*classes_loc=0x%02x", *classes_loc);
}


//
// Error Detail
//

DDCA_Error_Detail *
ddca_get_error_detail() {
   bool debug = false;
   DBGMSF(debug, "Starting");

   DDCA_Error_Detail * result = dup_error_detail(get_thread_error_detail());

   if (debug) {
      DBGMSG("Done.     Returning: %p", result);
      if (result)
         report_error_detail(result, 2);
   }
   return result;
}


void
ddca_free_error_detail(DDCA_Error_Detail * ddca_erec) {
   free_error_detail(ddca_erec);
}


void
ddca_report_error_detail(DDCA_Error_Detail * ddca_erec, int depth) {
   report_error_detail(ddca_erec, depth);
}


// DDCA_Error_Detail * ddca_dup_error_detail(DDCA_Error_Detail * original) {
//     return dup_error_detail(original);
// }


//
// Status Code Management
//

const char *
ddca_rc_name(DDCA_Status status_code) {
   char * result = NULL;
   Status_Code_Info * code_info = find_status_code_info(status_code);
   if (code_info)
      result = code_info->name;
   return result;
}


const char *
ddca_rc_desc(DDCA_Status status_code) {
   char * result = "unknown status code";
   Status_Code_Info * code_info = find_status_code_info(status_code);
   if (code_info)
      result = code_info->description;
   return result;
}


//
// Output redirection
//

// Redirects output that normally would go to STDOUT
void
ddca_set_fout(FILE * fout) {
   // DBGMSG("Starting. fout=%p", fout);
   set_fout(fout);
}


void
ddca_set_fout_to_default(void) {
   set_fout_to_default();
}


// Redirects output that normally would go to STDERR
void
ddca_set_ferr(FILE * ferr) {
   set_ferr(ferr);
}


void
ddca_set_ferr_to_default(void) {
   set_ferr_to_default();
}


//
// Output capture - convenience functions
//

void
ddca_start_capture(DDCA_Capture_Option_Flags flags) {
   start_capture(flags);
}


char *
ddca_end_capture(void) {
   return end_capture();
}



//
// Message Control
//

DDCA_Output_Level
ddca_get_output_level(void) {
   return get_output_level();
}


DDCA_Output_Level
ddca_set_output_level(DDCA_Output_Level newval) {
     return set_output_level(newval);
}


char *
ddca_output_level_name(DDCA_Output_Level val) {
   return output_level_name(val);
}


//
// Global Settings
//

#ifdef REMOVED
int
ddca_max_max_tries(void) {
   return MAX_MAX_TRIES;
}


//  *** THIS IS FOR THE CURRENT THREAD
//  *** replace using function specifying display
//  *** for now, revert to old try_data_get_maxtries2()
int
ddca_get_max_tries(DDCA_Retry_Type retry_type) {
   // stats for multi part writes and reads are separate, but the
   // max tries for both are identical
// #ifndef NDEBUG
   Retry_Op_Value result3 = try_data_get_maxtries2((Retry_Operation) retry_type);
// #endif
   // // new way using retry_mgt
   // Retry_Op_Value result2 = trd_get_thread_max_tries((Retry_Operation) retry_type);
   // assert(result == result2);
   // assert(result2 == result3);
   return result3;
}


// ** THIS IS FOR CURRENT THREAD - FIX
DDCA_Status
ddca_set_max_tries(
      DDCA_Retry_Type retry_type,
      int             max_tries)
{
   DDCA_Status rc = 0;
   free_thread_error_detail();
   if (max_tries < 1 || max_tries > MAX_MAX_TRIES)
      rc = DDCRC_ARG;
   else {
      try_data_set_maxtries2((Retry_Operation) retry_type, max_tries);
      // for DDCA_MULTI_PART_TRIES, set both  MULTI_PART_WRITE_OP and MULTI_PART_READ_OP
      if (retry_type == DDCA_MULTI_PART_TRIES)
         try_data_set_maxtries2(MULTI_PART_WRITE_OP, max_tries);

      // new way, set in retry_mgt
#ifdef TRD
      trd_set_thread_max_tries((Retry_Operation) retry_type, max_tries);
      if (retry_type == DDCA_MULTI_PART_TRIES)
           trd_set_thread_max_tries(MULTI_PART_WRITE_OP, max_tries);
#endif
   }
   return rc;
}
#endif

bool
ddca_enable_verify(bool onoff) {
   return ddc_set_verify_setvcp(onoff);
}


bool
ddca_is_verify_enabled() {
   return ddc_get_verify_setvcp();
}


#ifdef REMOVED

// *** FOR CURRENT THREAD
double
ddca_set_default_sleep_multiplier(double multiplier)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API, "Setting multiplier = %6.3f", multiplier);

   double old_value = -1.0;
   if (multiplier >= 0.0 && multiplier <= 10.0) {
// #ifdef TSD
      old_value = pdd_get_default_sleep_multiplier_factor();
      pdd_set_default_sleep_multiplier_factor(multiplier, Reset);
// #endif
    }

   DBGTRC_DONE(debug, DDCA_TRC_API, "Returning: %6.3f", old_value);
   return old_value;
}


double
ddca_get_default_sleep_multiplier()
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API, "");
   double result = pdd_get_default_sleep_multiplier_factor();
   DBGTRC(debug, DDCA_TRC_API, "Returning %6.3f", result);
   return result;
}


void
ddca_set_global_sleep_multiplier(double multiplier)
{
   ddca_set_default_sleep_multiplier(multiplier);
   return;
}

double
ddca_get_global_sleep_multiplier()
{
   return ddca_get_default_sleep_multiplier();
}
#endif



// for display on current thread
double
ddca_set_sleep_multiplier(double multiplier)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API, "Setting multiplier = %6.3f", multiplier);

   double old_value = -1.0;
   if (multiplier >= 0.0 && multiplier <= 10.0) {
      Per_Thread_Data * ptd = ptd_get_per_thread_data();
      if (ptd->cur_dh) {
         Per_Display_Data * pdd = ptd->cur_dh->dref->pdd;
         old_value = pdd->user_sleep_multiplier;
         pdd_reset_multiplier(pdd, multiplier);
      }
   }

   DBGTRC_DONE(debug, DDCA_TRC_API, "Returning: %6.3f", old_value);
   return old_value;
}

double
ddca_get_sleep_multiplier()
{
   bool debug = false;
   DBGTRC(debug, DDCA_TRC_API, "");

   Per_Thread_Data * ptd = ptd_get_per_thread_data();
   double result =  -1.0f;
   if (ptd->cur_dh) {
      Per_Display_Data * pdd = ptd->cur_dh->dref->pdd;
      result = pdd->user_sleep_multiplier;
   }
#ifdef TSD
   double result = tsd_get_sleep_multiplier_factor();
#endif
   DBGTRC(debug, DDCA_TRC_API, "Returning %6.3f", result);
   return result;
}

#ifdef RELEASE_2_1_0
double
ddca_set_sleep_multiplier(double multiplier)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_API, "Setting multiplier = %6.3f", multiplier);

   double old_value = -1.0;
   if (multiplier >= 0.0 && multiplier <= 10.0) {
      Per_Thread_Data * ptd = ptd_get_per_thread_data();
      old_value = ptd->sleep_multiplier;
      ptd->sleep_multiplier = multiplier;
   }

   DBGTRC_DONE(debug, DDCA_TRC_API, "Returning: %6.3f", old_value);
   return old_value;
}

double
ddca_get_sleep_multiplier()
{
   bool debug = false;
   DBGTRC(debug, DDCA_TRC_API, "");

   Per_Thread_Data * ptd = ptd_get_per_thread_data();
   double result = ptd->sleep_multiplier;

   DBGTRC(debug, DDCA_TRC_API, "Returning %6.3f", result);
   return result;
}
#endif

#ifdef FUTURE

/** Gets the I2C timeout in milliseconds for the specified timeout class.
 * @param timeout_type timeout type
 * @return timeout in milliseconds
 */
int
ddca_get_timeout_millis(
      DDCA_Timeout_Type timeout_type) {
   return 0;    // *** UNIMPLEMENTED ***
}

/** Sets the I2C timeout in milliseconds for the specified timeout class
 * @param timeout_type  timeout class
 * @param millisec      timeout to set, in milliseconds
 */
void
ddca_set_timeout_millis(
      DDCA_Timeout_Type timeout_type,
      int               millisec)
{
   // *** UNIMPLEMENTED
}
#endif


#ifdef REMOVED

/** Controls the force I2C slave address setting.
 *
 *  Normally, ioctl operation I2C_SLAVE is used to set the I2C slave address.
 *  If that returns EBUSY and this setting is in effect, slave address setting
 *  is retried using operation I2C_SLAVE_FORCE.
 *
 *  @param[in] onoff true/false
 *  @return  prior value
 *  @since 1.2.2
 */
bool
ddca_enable_force_slave_address(bool onoff);

/** Query the force I2C slave address setting.
 *
 *  @return true/false
 *  @since 1.2.2
 */
bool
ddca_is_force_slave_address_enabled(void);
#endif

#ifdef REMOVED
bool
ddca_enable_force_slave_address(bool onoff) {
   bool old = i2c_forceable_slave_addr_flag;
   i2c_forceable_slave_addr_flag = onoff;
   return old;
}


bool
ddca_is_force_slave_address_enabled(void) {
   return i2c_forceable_slave_addr_flag;
}
#endif


//
// Statistics
//

// TODO: Add functions to access ddcutil's runtime error statistics

void
ddca_reset_stats(void) {
   // DBGMSG("Executing");
   ddc_reset_stats_main();
}

// TODO: Functions that return stats in data structures
void
ddca_show_stats(
      DDCA_Stats_Type stats_types,
      bool            per_display_stats,
      int             depth)
{
   if (stats_types)
      ddc_report_stats_main( stats_types, per_display_stats, per_display_stats, false, depth);
}

void
ddca_report_locks(
      int             depth)
{
   dbgrpt_display_locks(depth);
}


void init_api_base() {
   // DBGMSG("Executing");
   RTTI_ADD_FUNC(_ddca_terminate);
   RTTI_ADD_FUNC(ddca_start_watch_displays);
   RTTI_ADD_FUNC(ddca_stop_watch_displays);
   RTTI_ADD_FUNC(ddca_get_active_watch_classes);
#ifdef REMOVED
   RTTI_ADD_FUNC(ddca_set_sleep_multiplier);
   RTTI_ADD_FUNC(ddca_set_default_sleep_multiplier);
#endif
}

