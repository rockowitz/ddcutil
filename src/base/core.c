/** @file core.c
 * Core functions and global variables.
 *
 * File core.c provides a collection of inter-dependent services at the core
 * of the **ddcutil** application.
 *
 * These include
 * - message destination redirection
 * - abnormal termination
 * - standard function call options
 * - timestamp generation
 * - message level control
 * - debug and trace messages
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#define GNU_SOURCE    // for syscall()

//* \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef TARGET_BSD
#include <pthread_np.h>
#else
#include <sys/types.h>
#include <sys/syscall.h>
#endif
#include <unistd.h>
/** \endcond */

#include "util/data_structures.h"
#include "util/debug_util.h"
#include "util/file_util.h"
#include "util/glib_util.h"
#include "util/glib_string_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/timestamp.h"

#include "base/ddc_errno.h"
#include "base/linux_errno.h"

#include "base/core.h"


//
// Global SDTOUT and STDERR redirection, for controlling message output in API
//

/** @defgroup output_redirection Basic Output Redirection
 */

/** Current stream for normal output.
 *
 * @remark
 * Will be NULL until init_msg_control() is called.
 * Be careful during program initialization.
 *
 * @ingroup output_redirection
 */
// static FILE * FOUT = NULL;

/** Current stream for error messages.
 *
 * @remark
 * Will be NULL until init_msg_control() is called.
 * Be careful during program initialization.
 *
 * @ingroup output_redirection
 */
// static FILE * FERR = NULL;


#ifdef OVERKILL
#define FOUT_STACK_SIZE 8

static FILE* fout_stack[FOUT_STACK_SIZE];
static int   fout_stack_pos = -1;
#endif


typedef struct {
   FILE * fout;
   FILE * ferr;
   DDCA_Output_Level output_level;
   // bool   report_ddc_errors;    // unused, ddc error reporting left as global
   DDCA_Error_Detail * error_detail;
} Thread_Output_Settings;

static Thread_Output_Settings *  get_thread_settings() {
   static GPrivate per_thread_dests_key = G_PRIVATE_INIT(g_free);

   Thread_Output_Settings *settings = g_private_get(&per_thread_dests_key);

   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, settings=%p\n", __func__, this_thread, settings);

   if (!settings) {
      settings = g_new0(Thread_Output_Settings, 1);
      settings->fout = stdout;
      settings->ferr = stderr;
      settings->output_level = DDCA_OL_NORMAL;
      // settings->report_ddc_errors = false;    // redundant, but specified for documentation

      g_private_set(&per_thread_dests_key, settings);
   }

   // printf("(%s) Returning: %p\n", __func__, settings);
   return settings;
}



/** Initialize **stdout** and **stderr** redirection.
 *
 * Must be called during program initialization.
 *
 * @ingroup output_redirection
 */
void init_msg_control() {
   // FOUT = stdout;
   // FERR = stderr;

   // initialization now performed in get_thread_settings()
}


// issue: how to specify that output should be discarded vs reset to stdout?
// issue will resetting report dest cause conflicts?
// To reset to STDOUT, use constant stdout in stdio.h  - NO - screws up rpt_util
// problem:

/** Redirect output on the current thread that would normally go to **stdout**.
 *
 *  @param fout pointer to output stream
 *
 * @ingroup output_redirection
 */
void set_fout(FILE * fout) {
   bool debug = false;
   DBGMSF(debug, "fout = %p", fout);
   Thread_Output_Settings * dests = get_thread_settings();
   dests->fout = fout;
   // FOUT = fout;
   rpt_change_output_dest(fout);
}

/** Redirect output that would normally go to **stdout** back to **stdout**.
 * @ingroup output_redirection
 */
void set_fout_to_default() {
   // FOUT = stdout;
   Thread_Output_Settings * dests = get_thread_settings();
   dests->fout = stdout;
   rpt_change_output_dest(stdout);
}

/** Redirect output that would normally go to **stderr**..
 *
 *  @param ferr pointer to output stream
 * @ingroup output_redirection
 */
void set_ferr(FILE * ferr) {
   // FERR = ferr;
   Thread_Output_Settings * dests = get_thread_settings();
   dests->ferr = ferr;
}

/** Redirect output that would normally go to **stderr** back to **stderr**.
 * @ingroup output_redirection
 */
void set_ferr_to_default() {
   // FERR = stderr;
   Thread_Output_Settings * dests = get_thread_settings();
   dests->ferr = stderr;
}

FILE * fout() {
   // return FOUT;
   Thread_Output_Settings * dests = get_thread_settings();
   return dests->fout;
}

FILE * ferr() {
   // return FERR;
   Thread_Output_Settings * dests = get_thread_settings();
   return dests->ferr;
}


#ifdef OVERKILL

// Functions that allow for temporarily changing the output destination.


void push_fout(FILE* new_dest) {
   assert(fout_stack_pos < FOUT_STACK_SIZE-1);
   fout_stack[++fout_stack_pos] = new_dest;
}


void pop_fout() {
   if (fout_stack_pos >= 0)
      fout_stack_pos--;
}


void reset_fout_stack() {
   fout_stack_pos = 0;
}


FILE * cur_fout() {
   // special handling for unpushed case because can't statically initialize
   // output_dest_stack[0] to stdout
   return (fout_stack_pos < 0) ? stdout : fout_stack[fout_stack_pos];
}
#endif


//
// Standard call options
//

Value_Name_Table callopt_bitname_table2 = {
      VN(CALLOPT_ERR_MSG),
 //   VN(CALLOPT_ERR_ABORT),
      VN(CALLOPT_RDONLY),
      VN(CALLOPT_WARN_FINDEX),
      VN(CALLOPT_FORCE),
      VN(CALLOPT_NONE),                // special entry
      VN_END
};


/** Interprets a **Call_Options** byte as a printable string.
 *  The returned value is valid until the next call of this function in
 *  the current thread.
 *
 *  @param calloptions  **Call_Options** byte
 *
 *  @return interpreted value
 */
char * interpret_call_options_t(Call_Options calloptions) {
   static GPrivate  buf_key = G_PRIVATE_INIT(g_free);
   char * buf = get_thread_fixed_buffer(&buf_key, 100);

   char * buftemp = vnt_interpret_flags(calloptions, callopt_bitname_table2, false, "|");
   g_strlcpy(buf, buftemp, 200);
   free(buftemp);

   return buf;
}


// Local definitions and functions shared by all message control categories

#define SHOW_REPORTING_TITLE_START 0
#define SHOW_REPORTING_MIN_TITLE_SIZE 28


static void
print_simple_title_value(int    offset_start_to_title,
                         char * title,
                         int    offset_title_start_to_value,
                         char * value)
{
   f0printf(fout(), "%.*s%-*s%s\n",
            offset_start_to_title,"",
            offset_title_start_to_value, title,
            value);
   fflush(fout());
}


//
// Message level control for normal output
//

/** \defgroup msglevel Message Level Management
 *
 * Functions and variables to manage and query output level settings.
 */

// static DDCA_Output_Level output_level;


/** Gets the current output level.
 *
 * @return output level
 *
 * \ingroup msglevel
 */
DDCA_Output_Level get_output_level() {
   // return output_level;
   Thread_Output_Settings * settings = get_thread_settings();
   return settings->output_level;
}


/** Sets the output level.
 *
 * @param newval output level to set
 * @return old output level
 *
 *  \ingroup msglevel
 */
DDCA_Output_Level set_output_level(DDCA_Output_Level newval) {
   // printf("(%s) newval=%s  \n", __func__, msgLevelName(newval) );
   // output_level = newval;
   Thread_Output_Settings * settings = get_thread_settings();
   DDCA_Output_Level old_level = settings->output_level;
   settings->output_level = newval;
   return old_level;
}


/** Gets the printable name of an output level.
 *
 * @param val  output level
 * @return printable name for output level
 *
 *  \ingroup msglevel
 */
char * output_level_name(DDCA_Output_Level val) {
   char * result = NULL;
   switch (val) {
      case DDCA_OL_TERSE:
         result = "Terse";
         break;
      case DDCA_OL_NORMAL:
         result = "Normal";
         break;
      case DDCA_OL_VERBOSE:
         result = "Verbose";
         break;
      case DDCA_OL_VV:
         result = "Very Vebose";
      // default unnecessary, case exhausts enum
   }
   // printf("(%s) val=%d 0x%02x, returning: %s\n", __func__, val, val, result);
   return result;
}


/** Reports the current output level.
 *  The report is written to the current **FOUT** device.
 *
 *  \ingroup msglevel
 */
void show_output_level() {
   // printf("Output level:           %s\n", output_level_name(output_level));
   Thread_Output_Settings * settings = get_thread_settings();
   print_simple_title_value(SHOW_REPORTING_TITLE_START,
                              "Output level: ",
                              SHOW_REPORTING_MIN_TITLE_SIZE,
                              output_level_name(settings->output_level));
}


//
// Debug and trace message control
//

/** defgroup dbgtrace Debug and Trace Messages
 *
 */

bool dbgtrc_show_time      = false;   ///< include elapsed time in debug/trace output
bool dbgtrc_show_thread_id = false;   ///< include thread id in debug/trace output

static
Value_Name_Title_Table trace_group_table = {
      VNT(DDCA_TRC_BASE,  "BASE"),
      VNT(DDCA_TRC_I2C,   "I2C"),
#ifdef HAVE_ADL
      VNT(TRC_ADL, "ADL"),
#endif
      VNT(DDCA_TRC_DDC,   "DDC"),
      VNT(DDCA_TRC_USB,   "USB"),
      VNT(DDCA_TRC_TOP,   "TOP"),
      VNT(DDCA_TRC_ENV,   "ENV"),
      VNT(DDCA_TRC_API,   "API"),
      VNT(DDCA_TRC_UDF,   "UDF"),
      VNT(DDCA_TRC_VCP,   "VCP"),
      VNT(DDCA_TRC_DDCIO, "DDCIO"),
      VNT(DDCA_TRC_SLEEP, "SLEEP"),
      VNT(DDCA_TRC_RETRY, "RETRY"),
      VNT_END
};
const int trace_group_ct = ARRAY_SIZE(trace_group_table)-1;


/** Given a trace group name, returns its identifier.
 *  Case is ignored.
 *
 *  @param name trace group name
 *  @return trace group identifier
 *  @retval  TRC_NEVER unrecognized name
 *
 *  /ingroup dbgtrace
 */
DDCA_Trace_Group trace_class_name_to_value(char * name) {
   return (DDCA_Trace_Group) vnt_find_id(
                           trace_group_table,
                           name,
                           true,      // search title field
                           true,      // ignore-case
                           DDCA_TRC_NONE);
}

static DDCA_Trace_Group trace_levels = DDCA_TRC_NONE;   // 0x00


/** Specifies the trace groups to be traced.
 *
 * @param trace_flags bit flags indicating groups to trace
 *
 * @ingroup dbgtrace
 */
void set_trace_levels(DDCA_Trace_Group trace_flags) {
   bool debug = false;
   DBGMSF(debug, "trace_flags=0x%04x\n", trace_flags);

   trace_levels = trace_flags;
}


// traced_function_table and traced_file_table were initially implemented using
// GHashTable.  The implementation had bugs, and given that (a) these data structures
// are used only for testing and (b) there will be at most a handful of entries in the
// tables, a simpler GPtrArray implementation is used.

static GPtrArray  * traced_function_table = NULL;
static GPtrArray  * traced_file_table     = NULL;


/** Adds a function to the list of functions to be traced.
 *
 *  @param funcname function name
 */
void add_traced_function(const char * funcname) {
   // printf("(%s) funcname=|%s|\n", __func__, funcname);

   if (!traced_function_table)
      traced_function_table = g_ptr_array_new();
   // n. g_ptr_array_find_with_equal_func() requires glib 2.54
   if (gaux_string_ptr_array_find(traced_function_table, funcname) < 0)
      g_ptr_array_add(traced_function_table, g_strdup(funcname));
}

/** Adds a file to the list of files to be traced.
 *
 *  @param filename file name
 *
 *  @remark
 *  Only the basename portion of the specified file name is used.
 *  @remark
 *  If the file name does not end in ".c", that suffix is appended.
 */
void add_traced_file(const char * filename) {
   if (!traced_file_table)
      traced_file_table = g_ptr_array_new();
   // n. g_ptr_array_find_with_equal_func() requires glib 2.54

   gchar * bname = g_path_get_basename(filename);
   if (!str_ends_with(bname, ".c")) {
      int newsz = strlen(bname) + 2 + 1;
      gchar * temp = calloc(1, newsz);
      strcpy(temp, bname);
      strcat(temp, ".c");
      free(bname);
      bname = temp;
   }

   if (gaux_string_ptr_array_find(traced_file_table, bname) < 0)
      g_ptr_array_add(traced_file_table, bname);
   else
      free(bname);
   // printf("(%s) filename=|%s|, bname=|%s|, found=%s\n", __func__, filename, bname, sbool(found));
}


/** Checks if a function is being traced.
 *
 *  @param funcname function name
 *  @return **true** if the function is being traced, **false** if not
 */
bool is_traced_function(const char * funcname) {
   bool result = (traced_function_table && gaux_string_ptr_array_find(traced_function_table, funcname) >= 0);
   // printf("(%s) funcname=|%s|, returning: %s\n", __func__, funcname, sbool(result2));
   return result;
}


/** Checks if a file is being traced.
 *
 *  @param filename file name
 *  @return **true** if trace is enabled for all functions in the file, **false** if not
 */
bool is_traced_file(const char * filename) {
   bool result = false;
   if (filename) {
      char * bname = g_path_get_basename(filename);
      result = (traced_file_table && gaux_string_ptr_array_find(traced_file_table, bname) >= 0);
      // printf("(%s) filename=|%s|, bname=|%s|, returning: %s\n", __func__, filename, bname, sbool(result));
      free(bname);
   }
   return result;
}


static char * get_traced_functions_as_joined_string() {
   char * result = NULL;
   if (traced_function_table) {
      g_ptr_array_sort(traced_function_table, gaux_ptr_scomp);
      result = join_string_g_ptr_array(traced_function_table, ", ");
   }
   return result;
}


static char * get_traced_files_as_joined_string() {
   char * result = NULL;
   if (traced_file_table) {
      g_ptr_array_sort(traced_file_table, gaux_ptr_scomp);
      result = join_string_g_ptr_array(traced_file_table, ", ");
   }
   return result;
}


/** Outputs a line reporting the traced function list.
 *  Output is written to the current **FOUT** device.
 */
void show_traced_functions() {
   char * buf = get_traced_functions_as_joined_string();
   print_simple_title_value(SHOW_REPORTING_TITLE_START,
                              "Traced functions: ",
                              SHOW_REPORTING_MIN_TITLE_SIZE,
                              (buf && (strlen(buf) > 0)) ? buf : "none");
   free(buf);
}


/** Outputs a line reporting the traced file list.
 *  Output is written to the current **FOUT** device.
 */
void show_traced_files() {
   char * buf = get_traced_files_as_joined_string();
   print_simple_title_value(SHOW_REPORTING_TITLE_START,
                              "Traced files: ",
                              SHOW_REPORTING_MIN_TITLE_SIZE,
                              (buf && (strlen(buf) > 0)) ? buf : "none");
   free(buf);
}


/** Checks if a tracing is to be performed.
 *
 * Tracing is enabled if any of the following tests pass:
 * - trace group
 * - file name
 * - function name
 *
 * @param trace_group group to check
 * @param filename    file from which check is occurring (not currently used)
 * @param funcname    function name
 *
 * @return **true** if tracing enabled, **false** if not
 *
 * @remark
 * - Multiple trace group bits can be set in **trace_group**.  If any of those
 *   group are currently being traced, the function returns **true**. That is,
 *   a given trace location in the code can be activated by multiple trace groups.
 * - If trace_group == TRC_ALWAYS (0xff), the function returns **true**.
 *   Commonly, if debugging is active in a given location, the trace_group value
 *   being checked can be set to TRC_ALWAYS, so a site can have a single debug/trace
 *   function call.
 *
 * @ingroup dbgtrace
 *
 */
bool is_tracing(DDCA_Trace_Group trace_group, const char * filename, const char * funcname) {
   bool result =  (trace_group == 0xff) || (trace_levels & trace_group); // is trace_group being traced?

   result = result || is_traced_function(funcname) || is_traced_file(filename);

   // printf("(%s) trace_group = %02x, filename=%s, funcname=%s, traceLevels=0x%02x, returning %d\n",
   //        __func__, trace_group, filename, funcname, trace_levels, result);
   return result;
}


/** Outputs a line reporting the active trace groups.
 *  Output is written to the current **FOUT** device.
 */
void show_trace_groups() {
   // DBGMSG("trace_levels: 0x%04x", trace_levels);
   char * buf = vnt_interpret_flags(trace_levels, trace_group_table, true /* use title */, ", ");
   print_simple_title_value(SHOW_REPORTING_TITLE_START,
                              "Trace groups active: ",
                              SHOW_REPORTING_MIN_TITLE_SIZE,
                              (strlen(buf) == 0) ? "none" : buf);
   free(buf);
}


//
// Error_Info reporting
//

/** If true, report #Error_Info instances before they are freed. */
bool report_freed_exceptions = false;


//
// Report DDC data errors
//

#ifdef PER_THREAD
bool enable_report_ddc_errors(bool onoff) {
   Thread_Output_Settings * dests = get_thread_settings();
   bool old_val = dests->report_ddc_errors;
   dests->report_ddc_errors = onoff;
   return old_val;
}

bool is_report_ddc_errors_enabled() {
   Thread_Output_Settings * dests = get_thread_settings();
   bool old_val = dests->report_ddc_errors;
   return old_val;
}
#else
static bool report_ddc_errors = false;
static GMutex report_ddc_errors_mutex;

bool enable_report_ddc_errors(bool onoff) {
   g_mutex_lock(&report_ddc_errors_mutex);
   bool old_val = report_ddc_errors;
   report_ddc_errors = onoff;
   g_mutex_unlock(&report_ddc_errors_mutex);
   return old_val;
}

bool is_report_ddc_errors_enabled() {
   bool old_val = report_ddc_errors;
   return old_val;
}
#endif


/** Checks if DDC data errors are to be reported.  This is the case if any of the
 *  following hold:
 *  - DDC error reporting has been explicitly enabled
 *  - The trace group specified by the calling function is currently active.
 *  - The value of **trace_group** is 0xff, which is the convention used for debug messages
 *  - The file name specified is currently being traced
 *  - The function name specified is currently being traced.
 *
 *  @param trace_group trace group of caller, if 0xff then always output
 *  @param filename file name to check
 *  @param funcname function name to check
 *  @return **true** if message is to be output, **false** if not
 *
 *  @remark
 *  This function is normally wrapped in function **IS_REPORTING_DDC()**
 */
bool is_reporting_ddc(DDCA_Trace_Group trace_group, const char * filename, const char * funcname) {
  bool result = (is_tracing(trace_group,filename, funcname) ||
#ifdef PER_THREAD
        is_report_ddc_errors_enabled()
#else
        report_ddc_errors
#endif
        );
  return result;
}


/* Submits a message regarding a DDC data error for possible output.
 *
 * @param trace_group group to check, 0xff to always output
 * @param funcname    function name to check
 * @param lineno      line number in file
 * @param filename    file name to check
 * @param format      message format string
 * @param ...         arguments for message format string
 *
 * @return **true** if message was output, **false** if not
 *
 * Normally, invocation of this function is wrapped in macro DDCMSG.
 */
bool ddcmsg(DDCA_Trace_Group  trace_group,
            const char * funcname,
            const int    lineno,
            const char * filename,
            char *       format,
            ...)
{
   bool result = false;
   bool debug_or_trace = is_tracing(trace_group, filename, funcname);
   if (debug_or_trace || is_report_ddc_errors_enabled()) {
      result = true;
      char buffer[200];
      va_list(args);
      va_start(args, format);
      vsnprintf(buffer, 200, format, args);
      if (debug_or_trace) {
         // f0printf(fout(), "(%s) DDC: %s\n", funcname, buffer);
         // use dbgtrc() for consistent handling of timestamp and thread id prefixes
         dbgtrc(0xff, funcname, lineno, filename, "DDC: %s", buffer);
      }
      else
         f0printf(fout(), "DDC: %s\n", buffer);
      fflush(fout());
      va_end(args);
   }
   return result;
}


/** Tells whether DDC data errors are reported.
 *  Output is written to the current **FOUT** device.
 */
void show_ddcmsg() {
   print_simple_title_value(SHOW_REPORTING_TITLE_START,
                              "Reporting DDC data errors: ",
                              SHOW_REPORTING_MIN_TITLE_SIZE,
                              sbool(report_ddc_errors));
}


/** Reports output levels for:
 *   - general output level (terse, verbose, etc)
 *   - DDC data errors
 *   - trace groups
 *   - traced functions
 *   - traced files
 *
 * Output is written to the current **FOUT** device.
 */
void show_reporting() {
   show_output_level();
   show_ddcmsg();
   show_trace_groups();
   show_traced_functions();
   show_traced_files();
   // f0puts("", fout());
}


//
// Issue messages of various types
//

/** Issues an error message.
 *  The message is written to the current FERR device.
 *
 *  @param funcname      function name of caller
 *  @param lineno        line number in caller
 *  @param filename      file name of caller
 *  @param format        format string for message
 *  @param ...           arguments for format string
 *
 *  @remark
 *  This function cannot map to dbgtrc(), since it writes to stderr, not stdout
 *  @remark
 *  n. used within macro **LOADFUNC** of adl_intf.c
 */
void severemsg(
        const char * funcname,
        const int    lineno,
        const char * filename,
        char *       format,
        ...)
{
      char buffer[200];
      char buf2[250];
      va_list(args);
      va_start(args, format);
      vsnprintf(buffer, 200, format, args);
      snprintf(buf2, 250, "(%s) %s\n", funcname, buffer);
      f0puts(buf2, ferr());
      fflush(ferr());
      va_end(args);
}


/** Core function for emitting debug or trace messages.
 *  Normally wrapped in a DBGMSG or TRCMSG macro to simplify calling.
 *
 *  The message is output if any of the following are true:
 *  - the trace_group specified is currently active
 *  - the value is trace group is 0xff
 *  - funcname is the name of a function being traced
 *  - filename is the name of a file being traced
 *
 *  @param trace_group   trace group of caller, 0xff to always output
 *  @param funcname      function name of caller
 *  @param lineno        line number in caller
 *  @param filename      file name of caller
 *  @param format        format string for message
 *  @param ...           arguments for format string
 *
 *  @return **true** if message was output, **false** if not
 */
bool dbgtrc(
        DDCA_Trace_Group  trace_group,
        const char *      funcname,
        const int         lineno,
        const char *      filename,
        char *            format,
        ...)
{
   bool msg_emitted = false;
   if ( is_tracing(trace_group, filename, funcname) ) {
      va_list(args);
      va_start(args, format);
      char * buffer = g_strdup_vprintf(format, args);
      va_end(args);

      char  elapsed_prefix[15] = "";
      if (dbgtrc_show_time)
         g_snprintf(elapsed_prefix, 15, "[%s]", formatted_elapsed_time());

      char thread_prefix[15] = "";
      if (dbgtrc_show_thread_id) {
#ifdef TARGET_BSD
      int tid = pthread_getthreadid_np();
#else
         pid_t tid = syscall(SYS_gettid);
#endif
         snprintf(thread_prefix, 15, "[%7jd]", (intmax_t) tid);  // is this proper format for pid_t
      }

      char * buf2 = g_strdup_printf("%s%s(%-30s) %s\n",
                                    thread_prefix, elapsed_prefix, funcname, buffer);

      f0puts(buf2, fout());    // no automatic terminating null
      fflush(fout());

      free(buffer);
      free(buf2);
      msg_emitted = true;
   }

   return msg_emitted;
}


//
// Standardized handling of exceptional conditions, including
// error messages and possible program termination.
//

/** Reports an IOCTL error.
 *  The message is written to the current **FERR** device.
 *
 * @param  ioctl_name  ioctl name
 * @param  errnum      errno value
 * @param  funcname    function name of error
 * @param  filename    file name of error
 * @param  lineno      line number of error
 */
void report_ioctl_error(
      const char * ioctl_name,
      int          errnum,
      const char * funcname,
      const char * filename,
      int          lineno)
{
   int errsv = errno;
   f0printf(ferr(), "(%s) Error in ioctl(%s), errno=%s\n",
           funcname, ioctl_name, linux_errno_desc(errnum) );
   fflush(ferr());
   errno = errsv;
}


/** Called when a condition that should be impossible has been detected.
 *  Issues messages to the current FERR device.
 *
 * This function is normally invoked using macro PROGRAM_LOGIC_ERROR()
 *
 *  @param  funcname    function name
 *  @param  lineno      line number in source file
 *  @param  fn          source file name
 *  @param  format      format string, as in printf()
 *  @param  ...         one or more substitution values for the format string
 *
 * @ingroup output_redirection
 */
void program_logic_error(
      const char * funcname,
      const int    lineno,
      const char * fn,
      char *       format,
      ...)
{
  // assemble the error message
  char buffer[200];
  va_list(args);
  va_start(args, format);
  vsnprintf(buffer, 200, format, args);
  va_end(args);

  // assemble the location message:
  char buf2[250];
  snprintf(buf2, 250, "Program logic error in function %s at line %d in file %s:\n",
                      funcname, lineno, fn);

  // don't combine into 1 line, might be very long.  just output 2 lines:
  FILE * f = ferr();
  f0puts(buf2,   f);
  f0puts(buffer, f);
  f0puts("\n",   f);
  fflush(f);
}


//
// DDCA_Error_Detail related functions
//

/** Frees a #DDCA_Error_Detail instance
 *
 *  @param  instance to free
 */
void
free_error_detail(DDCA_Error_Detail * ddca_erec)
{
   if (ddca_erec) {
      assert(memcmp(ddca_erec->marker, DDCA_ERROR_DETAIL_MARKER, 4) == 0);
      for (int ndx = 0; ndx < ddca_erec->cause_ct; ndx++) {
         free_error_detail(ddca_erec->causes[ndx]);
      }
      free(ddca_erec->detail);
      ddca_erec->marker[3] = 'x';
      free(ddca_erec);
   }
}


/** Converts an internal #Error_Info instance to a publicly visible #DDCA_Error_Detail
 *
 *  @param  erec  instance to convert
 *  @return new #DDCA_Error_Detail instance
 */
DDCA_Error_Detail *
error_info_to_ddca_detail(Error_Info * erec)
{
   bool debug = false;
   DBGMSF(debug, "Starting. erec=%p", erec);
   if (debug)
      errinfo_report(erec, 2);

   DDCA_Error_Detail * result = NULL;
   if (erec) {
      // ???
      int reqd_size = sizeof(DDCA_Error_Detail) + erec->cause_ct * sizeof(DDCA_Error_Detail*);
      result = calloc(1, reqd_size);
      memcpy(result->marker, DDCA_ERROR_DETAIL_MARKER, 4);
      result->status_code = erec->status_code;
      if (erec->detail)
         result->detail = strdup(erec->detail);
      for (int ndx = 0; ndx < erec->cause_ct; ndx++) {
         DDCA_Error_Detail * cause = error_info_to_ddca_detail(erec->causes[ndx]);
         result->causes[ndx] = cause;
      }
      result->cause_ct = erec->cause_ct;
   }

   DBGMSF(debug, "Done. Returning: %p", result);
   if (debug)
      report_error_detail(result, 2);
   return result;
}


/** Makes a deep copy of a #DDC_Error_Detail instance.
 *
 *  @param  old  instance to copy
 *  @return new copy
 */
DDCA_Error_Detail *
dup_error_detail(DDCA_Error_Detail * old) {
   bool debug = false;
   DBGMSF(debug, "Starting. old=%p", old);
   if (debug)
      report_error_detail(old, 2);

   DDCA_Error_Detail * result = NULL;
   if (old) {
      // ???
      int reqd_size = sizeof(DDCA_Error_Detail) + old->cause_ct * sizeof(DDCA_Error_Detail*);
      result = calloc(1, reqd_size);
      memcpy(result->marker, DDCA_ERROR_DETAIL_MARKER, 4);
      result->status_code = old->status_code;
      if (old->detail)
         result->detail = strdup(old->detail);
      for (int ndx = 0; ndx < old->cause_ct; ndx++) {
         DDCA_Error_Detail * cause = dup_error_detail(old->causes[ndx]);
         result->causes[ndx] = cause;
      }
      result->cause_ct = old->cause_ct;
   }

   DBGMSF(debug, "Done. Returning: %p", result);
   if (debug)
      report_error_detail(result, 2);
   return result;
}


/** Emits a detailed report of a #DDCA_Error_Detail struct.
 *  Output is written to the current report output destination.
 *
 *  @param ddca_erec  instance to report
 *  @param depth      logical indentation depth
 */
void report_error_detail(DDCA_Error_Detail * ddca_erec, int depth)
{
   if (ddca_erec) {
      rpt_vstring(depth, "status_code=%s, detail=%s", ddcrc_desc_t(ddca_erec->status_code), ddca_erec->detail);
      if (ddca_erec->cause_ct > 0) {
         rpt_label(depth,"Caused by: ");
         for (int ndx = 0; ndx < ddca_erec->cause_ct; ndx++) {
            struct ddca_error_detail * cause = ddca_erec->causes[ndx];
            report_error_detail(cause, depth+1);
         }
      }
   }
}


// Thread-specific functions

/** Frees the #DDCA_Error_Detail (if any) for the current thread.
 */
void free_thread_error_detail() {
   Thread_Output_Settings * settings = get_thread_settings();
   if (settings->error_detail) {
      free_error_detail(settings->error_detail);
      settings->error_detail = NULL;
   }
}


/** Gets the #DDCA_Error_Detail record for the current thread
 *
 *  @return #DDCA_Error_Detail instance, NULL if none
 */
DDCA_Error_Detail * get_thread_error_detail() {
   Thread_Output_Settings * settings = get_thread_settings();
   return settings->error_detail;
}


/** Set the #DDCA_Error_Detail record for the current thread.
 *
 *  @param error_detail  #DDCA_Error_Detail record to set
 */
void save_thread_error_detail(DDCA_Error_Detail * error_detail) {
   bool debug = false;
   DBGMSF(debug, "Starting. error_detail=%p", error_detail);
   if (debug)
      report_error_detail(error_detail, 2);

   Thread_Output_Settings * settings = get_thread_settings();
   if (settings->error_detail)
      free_error_detail(settings->error_detail);
   settings->error_detail = error_detail;

   DBGMSF(debug, "Done");
}


/** Gets the id number of the current thread
 *
 *  @ return  thread number
 */

intmax_t get_thread_id()
{
#ifdef TARGET_BSD
   int tid = pthread_getthreadid_np();
#else
   pid_t tid = syscall(SYS_gettid);
#endif
   return tid;
}

