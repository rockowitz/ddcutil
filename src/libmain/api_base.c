/** api_base.c
 *
 *  C API base functions.
 */

// Copyright (C) 2015-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>

#include "public/ddcutil_c_api.h"

#include "util/ddcutil_config_file.h"
#include "util/xdg_util.h"

#include "base/base_init.h"
#include "base/build_info.h"
#include "base/core.h"
#include "base/core_per_thread_settings.h"
#include "base/parms.h"
#include "base/per_thread_data.h"
#include "base/thread_retry_data.h"
#include "base/thread_sleep_data.h"
#include "base/tuned_sleep.h"

#include "cmdline/cmd_parser.h"
#include "cmdline/parsed_cmd.h"

// #include "i2c/i2c_bus_core.h"   // for testing watch_devices

#include "ddc/ddc_displays.h"
#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_services.h"
#include "ddc/ddc_try_stats.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_watch_displays.h"

#include "ddc/common_init.h"

#include "libmain/api_error_info_internal.h"
#include "libmain/api_base_internal.h"


//
// Precondition Failure
//

DDCA_Api_Precondition_Failure_Mode api_failure_mode = DDCA_PRECOND_STDERR_RETURN;

DDCA_Api_Precondition_Failure_Mode
ddca_set_precondition_failure_mode(
      DDCA_Api_Precondition_Failure_Mode failure_mode)
{
   DDCA_Api_Precondition_Failure_Mode old = api_failure_mode;
   api_failure_mode = failure_mode;
   return old;
}

DDCA_Api_Precondition_Failure_Mode
ddca_get_precondition_failure_mode()
{
   return api_failure_mode;
}


//
// Library Build Information
//

DDCA_Ddcutil_Version_Spec
ddca_ddcutil_version(void) {
   static DDCA_Ddcutil_Version_Spec vspec = {255,255,255};
   static bool vspec_init = false;

   if (!vspec_init) {
      int ct = sscanf(get_base_ddcutil_version(), "%hhu.%hhu.%hhu", &vspec.major, &vspec.minor, &vspec.micro);
      assert(ct == 3);
      vspec_init = true;
   }
   DBGMSG("Returning: %d.%d.%d", vspec.major, vspec.minor, vspec.micro);
   return vspec;
}


//  Returns the ddcutil version as a string in the form "major.minor.micro".
const char *
ddca_ddcutil_version_string(void) {
   return get_full_ddcutil_version();
}


// Indicates whether the ddcutil library was built with support for USB connected monitors. .
bool
ddca_built_with_usb(void) {
#ifdef USE_USB
   return true;
#else
   return false;
#endif
}


// Alternative to individual ddca_built_with...() functions.
// conciseness vs documentatbility
// how to document bits?   should doxygen doc be in header instead?

DDCA_Build_Option_Flags
ddca_build_options(void) {
   uint8_t result = 0x00;
#ifdef USE_USB
         result |= DDCA_BUILT_WITH_USB;
#endif
#ifdef FAILSIM_ENABLED
         result |= DDCA_BUILT_WITH_FAILSIM;
#endif
   // DBGMSG("Returning 0x%02x", result);
   return result;
}

#ifdef FUTURE
char * get_library_filename() {
   intmax_t pid = get_process_id();
   return NULL;
}
#endif



//
// Initialization
//

static
Parsed_Cmd * get_parsed_libmain_config() {
   bool debug = true;
   DBGMSF(debug, "Starting.");

   Parsed_Cmd * parsed_cmd = NULL;

   // dummy initial argument list so libddcutil is not a special case
   Null_Terminated_String_Array cmd_name_array = calloc(2, sizeof(char*));
   cmd_name_array[0] = "libddcutil";
   cmd_name_array[1] = NULL;

   GPtrArray* errmsgs = g_ptr_array_new_with_free_func(g_free);
   char ** new_argv = NULL;
   int     new_argc = 0;
   char *  untokenized_option_string = NULL;
   char *  config_fn;
   DBGMSF(debug, "Calling apply_config_file()");
   int apply_config_rc = apply_config_file(
                                 "libddcutil",  // use this section of config file
                                 1, cmd_name_array,
                                 &new_argc,
                                 &new_argv,
                                 &untokenized_option_string,
                                 &config_fn,
                                 errmsgs);
   DBGMSF(debug, "apply_config_file() returned: %d, new_argc=%d, new_argv=%p",
                 apply_config_rc, new_argc, new_argv);
   assert(apply_config_rc <= 0);
   assert( new_argc == ntsa_length(new_argv) );

   if (errmsgs->len > 0) {
      f0printf(ferr(), "Errors reading libddcutil configuration file %s:\n", config_fn);
      for (int ndx = 0; ndx < errmsgs->len; ndx++) {
         f0printf(fout(), "   %s\n", (char*) g_ptr_array_index(errmsgs, ndx));
      }
   }
   g_ptr_array_free(errmsgs, true);
   if (untokenized_option_string && strlen(untokenized_option_string) > 0)
      fprintf(fout(), "Applying libddcutil options from %s: %s\n", config_fn,
            untokenized_option_string);

   // Continue even if config file errors
   // if (apply_config_rc < 0)
   //    goto bye;

   assert(new_argc >= 1);
   DBGMSF(debug, "Calling parse_command()");
   parsed_cmd = parse_command(new_argc, new_argv, MODE_LIBDDCUTIL);
   if (!parsed_cmd) {
      fprintf(ferr(), "Ignoring invalid configuration file options: %s\n",
                      untokenized_option_string);
      // fprintf(ferr(), "Terminating execution\n");
      // exit(1);
      DBGMSF(debug, "Retrying parse_command() with no options");
      parsed_cmd = parse_command(1, cmd_name_array, MODE_LIBDDCUTIL);
   }
   if (debug)
      dbgrpt_parsed_cmd(parsed_cmd, 1);
   free(untokenized_option_string);
   free(config_fn);

   DBGMSF(debug, "Done.     Returning %p", parsed_cmd);
   return parsed_cmd;
}


#ifdef TESTING_CLEANUP
void done() {
   printf("(%s) Starting\n", __func__);
   _ddca_terminate();
   syslog(LOG_INFO, "(%s) executing done()", __func__);
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

static FILE * flog = NULL;

bool library_initialized = false;

/** Initializes the ddcutil library module.
 *
 *  Normally called automatically when the shared library is loaded.
 *
 *  It is not an error if this function is called more than once.
 */
void __attribute__ ((constructor))
_ddca_init(void) {
   bool debug = true;
   if (debug)
      printf("(%s) Starting library_initialized=%s\n", __func__, sbool(library_initialized));
   if (!library_initialized) {
      openlog("libddcutil", LOG_CONS|LOG_PID, LOG_USER);
      syslog(LOG_INFO, "Initializing.  ddcutil version %s", get_full_ddcutil_version());
#ifdef TESTING_CLEANUP
      // signal(SIGTERM, dummy_sigterm_handler);
      // atexit(atexit_func);  // TESTING CLAEANUP
#endif
      init_base_services();
      Parsed_Cmd* parsed_cmd = get_parsed_libmain_config();
      init_tracing(parsed_cmd);
      if (parsed_cmd->s1 || parsed_cmd->library_trace_file) {
         // convoluted code because --s1 was not described to user as resolving
         // relative file name per XDG data spec
         // to be simplified before release
         char * trace_file = NULL;
         if (parsed_cmd->s1)     // vestigial from testing
            trace_file = strdup(parsed_cmd->s1);
         if (!trace_file && parsed_cmd->library_trace_file) {
            if (parsed_cmd->library_trace_file[0] != '/')
               trace_file = xdg_data_home_file("ddcutil", parsed_cmd->library_trace_file);
            else
               trace_file = strdup(parsed_cmd->library_trace_file);
         }
         if (debug)
            printf("(%s) Setting trace destination %s\n", __func__, trace_file);
         syslog(LOG_INFO, "Trace destination: %s", trace_file);
         flog = fopen(parsed_cmd->s1, "a+");
         if (flog) {
            time_t trace_start_time = time(NULL);
            char * trace_start_time_s = asctime(localtime(&trace_start_time));
            if (trace_start_time_s[strlen(trace_start_time_s)-1] == 0x0a)
                 trace_start_time_s[strlen(trace_start_time_s)-1] = 0;
            fprintf(flog, "%s tracing started %s\n", "libddcutil", trace_start_time_s);
            if (debug) {
               // to do: get absolute file name
               fprintf(stdout, "Writing %s trace output to %s\n", "libddcutil",parsed_cmd->s1);

            }
            set_default_thread_output_settings(flog, flog);
            set_fout(flog);
            set_ferr(flog);
         }
         else {
            fprintf(stderr, "Error opening libddcutil trace file: %s\n", strerror(errno));
         }
         free(trace_file);
      }
      submaster_initializer(parsed_cmd);
      // init_ddc_services();

     //  explicitly set the async threshold for testing
     //  int threshold = DISPLAY_CHECK_ASYNC_THRESHOLD_STANDARD;
     //  int threshold = DISPLAY_CHECK_ASYNC_NEVER; //
     //  ddc_set_async_threshold(threshold);

      // no longer needed, values are initialized on first use per-thread
      // set_output_level(DDCA_OL_NORMAL);
      // enable_report_ddc_errors(false);

      // dummy_display_change_handler() will issue messages if display is added or removed
      ddc_start_watch_displays();

      library_initialized = true;

#ifdef TESTING_CLEANUP
      // int atexit_rc = atexit(done);   // TESTING CLEANUP
      // printf("(%s) atexit() returned %d\n", __func__, atexit_rc);
#endif

      DBGTRC(debug, DDCA_TRC_API, "library initialization executed");
   }
   else {
      DBGTRC(debug, DDCA_TRC_API, "library was already initialized");
   }
   // TRACED_ASSERT(1==5); for testing
}


/** Cleanup at library termination
 *
 *  - Terminates thread that watches for display addition or removal.
 *  - Releases heap memory to avoid error reports from memory analyzers.
 */
void __attribute__ ((destructor))
_ddca_terminate(void) {
   bool debug = false;
   if (library_initialized) {
      DBGTRC(debug, DDCA_TRC_API, "Starting. library_initialized = true");
      release_base_services();
      ddc_stop_watch_displays();
      library_initialized = false;
      if (flog)
         fclose(flog);
      if (debug)
         printf("(%s) library termination executed\n", __func__);
   }
   else {
      if (debug)
         printf("(%s) library was already terminated\n", __func__);   // should be impossible"
   }
   syslog(LOG_INFO, "(%s) Terminating.", __func__);
   closelog();
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


void ddca_report_error_detail(DDCA_Error_Detail * ddca_erec, int depth) {
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


// quick and dirty for now
// TODO: make thread safe, wrap in mutex
bool
ddca_enable_error_info(bool enable) {
   bool old_value = report_freed_exceptions;
   report_freed_exceptions = enable;            // global in core.c
   return old_value;
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

typedef struct {
   FILE * in_memory_file;
   char * in_memory_bufstart; ;
   size_t in_memory_bufsize;
   DDCA_Capture_Option_Flags flags;
} In_Memory_File_Desc;


static In_Memory_File_Desc *
get_thread_capture_buf_desc() {
   static GPrivate  in_memory_key = G_PRIVATE_INIT(g_free);

   In_Memory_File_Desc* fdesc = g_private_get(&in_memory_key);

   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, fdesc=%p\n", __func__, this_thread, fdesc);

   if (!fdesc) {
      fdesc = g_new0(In_Memory_File_Desc, 1);
      g_private_set(&in_memory_key, fdesc);
   }

   // printf("(%s) Returning: %p\n", __func__, fdesc);
   return fdesc;
}


void
ddca_start_capture(DDCA_Capture_Option_Flags flags) {
   In_Memory_File_Desc * fdesc = get_thread_capture_buf_desc();

   if (!fdesc->in_memory_file) {
      fdesc->in_memory_file = open_memstream(&fdesc->in_memory_bufstart, &fdesc->in_memory_bufsize);
      ddca_set_fout(fdesc->in_memory_file);   // n. ddca_set_fout() is thread specific
      fdesc->flags = flags;
      if (flags & DDCA_CAPTURE_STDERR)
         ddca_set_ferr(fdesc->in_memory_file);
   }
   // printf("(%s) Done.\n", __func__);
}


char *
ddca_end_capture(void) {
   In_Memory_File_Desc * fdesc = get_thread_capture_buf_desc();
   // In_Memory_File_Desc * fdesc = &in_memory_file_desc;

   char * result = "\0";
   // printf("(%s) Starting.\n", __func__);
   assert(fdesc->in_memory_file);
   if (fflush(fdesc->in_memory_file) < 0) {
      ddca_set_ferr_to_default();
      SEVEREMSG("flush() failed. errno=%d", errno);
      return strdup(result);
   }
   // n. open_memstream() maintains a null byte at end of buffer, not included in in_memory_bufsize
   result = strdup(fdesc->in_memory_bufstart);
   if (fclose(fdesc->in_memory_file) < 0) {
      ddca_set_ferr_to_default();
      SEVEREMSG("fclose() failed. errno=%d", errno);
      return result;
   }
   // free(fdesc->in_memory_file); // double free, fclose() frees in memory file
   fdesc->in_memory_file = NULL;
   ddca_set_fout_to_default();
   if (fdesc->flags & DDCA_CAPTURE_STDERR)
      ddca_set_ferr_to_default();

   // printf("(%s) Done. result=%p\n", __func__, result);
   return result;
}


#ifdef UNUSED
/** Returns the current size of the in-memory capture buffer.
 *
 *  @return number of characters in current buffer, plus 1 for
 *          terminating null
 *  @retval -1 no capture buffer on current thread
 *
 *  @remark defined and tested but does not appear useful
 */
int ddca_captured_size() {
   // printf("(%s) Starting.\n", __func__);
   In_Memory_File_Desc * fdesc = get_thread_capture_buf_desc();

   int result = -1;
   // n. open_memstream() maintains a null byte at end of buffer, not included in in_memory_bufsize
   if (fdesc->in_memory_file) {
      fflush(fdesc->in_memory_file);
      result = fdesc->in_memory_bufsize + 1;   // +1 for trailing \0
   }
   // printf("(%s) Done. result=%d\n", __func__, result);
   return result;
}
#endif


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


bool
ddca_enable_report_ddc_errors(bool onoff) {
   return enable_report_ddc_errors(onoff);
}


bool
ddca_is_report_ddc_errors_enabled(void) {
   return is_report_ddc_errors_enabled();
}


//
// Global Settings
//

int
ddca_max_max_tries(void) {
   return MAX_MAX_TRIES;
}


int
ddca_get_max_tries(DDCA_Retry_Type retry_type) {
   // stats for multi part writes and reads are separate, but the
   // max tries for both are identical
   Retry_Op_Value result3 = try_data_get_maxtries2((Retry_Operation) retry_type);
   // new way using retry_mgt
   Retry_Op_Value result2 = trd_get_thread_max_tries((Retry_Operation) retry_type);
   // assert(result == result2);
   assert(result2 == result3);
   return result2;
}


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
      trd_set_thread_max_tries((Retry_Operation) retry_type, max_tries);
      if (retry_type == DDCA_MULTI_PART_TRIES)
           trd_set_thread_max_tries(MULTI_PART_WRITE_OP, max_tries);
   }
   return rc;
}


bool
ddca_enable_verify(bool onoff) {
   return ddc_set_verify_setvcp(onoff);
}


bool
ddca_is_verify_enabled() {
   return ddc_get_verify_setvcp();
}

#ifdef NOT_NEEDED
void ddca_lock_default_sleep_multiplier() {
   lock_default_sleep_multiplier();
}

void ddca_unlock_sleep_multiplier() {
   unlock_default_sleep_multiplier();
}
#endif


bool
ddca_enable_sleep_suppression(bool newval) {
   bool old = is_sleep_suppression_enabled();
   enable_sleep_suppression(newval);
   return old;
}

bool
ddca_is_sleep_suppression_enabled() {
   return is_sleep_suppression_enabled();
}


double
ddca_set_default_sleep_multiplier(double multiplier)
{
   double result = tsd_get_default_sleep_multiplier_factor();
   tsd_set_default_sleep_multiplier_factor(multiplier);
   return result;
}

double
ddca_get_default_sleep_multiplier()
{
   return tsd_get_default_sleep_multiplier_factor();
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

// for current thread
double
ddca_set_sleep_multiplier(double multiplier)
{
   // bool debug = false;
   double result = tsd_get_sleep_multiplier_factor();
   // DBGMSF(debug, "Setting %5.2f", multiplier);
   tsd_set_sleep_multiplier_factor(multiplier);
   // DBGMSF(debug, "Done");
   return result;
}

double
ddca_get_sleep_multiplier()
{
   // bool debug = false;
   // DBGMSF(debug, "Starting");
   double result = tsd_get_sleep_multiplier_factor();
   // DBGMSF(debug, "Returning %5.2f", result);
   return result;
}


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


//
// Tracing
//

void
ddca_add_traced_function(const char * funcname) {
   add_traced_function(funcname);
}


void
ddca_add_traced_file(const char * filename) {
   add_traced_file(filename);
}


void
ddca_set_trace_groups(DDCA_Trace_Group trace_flags) {
   set_trace_levels(trace_flags);
}


DDCA_Trace_Group
ddca_trace_group_name_to_value(char * name) {
   return trace_class_name_to_value(name);
}

void
ddca_set_trace_options(DDCA_Trace_Options  options) {
   // DBGMSG("options = 0x%02x", options);
   // global variables in core.c

   if (options & DDCA_TRCOPT_TIMESTAMP)
      dbgtrc_show_time = true;
   if (options & DDCA_TRCOPT_THREAD_ID)
      dbgtrc_show_thread_id = true;
}


//
// Statistics
//

// TODO: Add functions to access ddcutil's runtime error statistics


#ifdef UNUSED
void
ddca_register_thread_dref(DDCA_Display_Ref dref) {
   ptd_register_thread_dref( (Display_Ref *) dref);
}
#endif

void
ddca_set_thread_description(
      const char * description)
{
   ptd_set_thread_description( description );
}

void
ddca_append_thread_description(
      const char * description)
{
   ptd_append_thread_description(description);
}

const char *
ddca_get_thread_descripton() {
   return ptd_get_thread_description_t();
}

void
ddca_reset_stats(void) {
   // DBGMSG("Executing");
   ddc_reset_stats_main();
}

// TODO: Functions that return stats in data structures
void
ddca_show_stats(
      DDCA_Stats_Type stats_types,
      bool            by_thread,
      int             depth)
{
   if (stats_types)
      ddc_report_stats_main( stats_types, by_thread, depth);
}


