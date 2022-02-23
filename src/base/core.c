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

// Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include <syslog.h>
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

#include "base/build_info.h"
#include "base/core_per_thread_settings.h"
#include "base/ddc_errno.h"
#include "base/linux_errno.h"

#include "base/core.h"


//
// Standard call options
//

Value_Name_Table callopt_bitname_table2 = {
      VN(CALLOPT_ERR_MSG),
 //   VN(CALLOPT_ERR_ABORT),
      VN(CALLOPT_RDONLY),
      VN(CALLOPT_WARN_FINDEX),
      VN(CALLOPT_FORCE),
      VN(CALLOPT_WAIT),
      VN(CALLOPT_FORCE_SLAVE_ADDR),
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
   g_strlcpy(buf, buftemp, 200);    // n. this is a debug msg, truncation benign
   free(buftemp);

   return buf;
}


// Local definitions and functions shared by all message control categories

#define SHOW_REPORTING_TITLE_START 0
#define SHOW_REPORTING_MIN_TITLE_SIZE 28


static void
print_simple_title_value(int    offset_start_to_title,
                         const char * title,
                         int    offset_title_start_to_value,
                         const char * value)
{
   f0printf(fout(), "%.*s%-*s%s\n",
            offset_start_to_title,"",
            offset_title_start_to_value, title,
            value);
   fflush(fout());
}




/** Reports the output level for the current thread
 *  The report is written to the current **FOUT** device.
 *
 *  \ingroup msglevel
 */
void show_output_level() {
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
bool dbgtrc_show_wall_time = false;   ///< include wall time in debug/trace output
bool dbgtrc_show_thread_id = false;   ///< include thread id in debug/trace output

static
Value_Name_Title_Table trace_group_table = {
      VNT(DDCA_TRC_BASE,  "BASE"),
      VNT(DDCA_TRC_I2C,   "I2C"),
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

/** Replaces the groups to be traced.
 *
 * @param trace_flags bit flags indicating groups to trace
 *
 * @ingroup dbgtrace
 */
void set_trace_groups(DDCA_Trace_Group trace_flags) {
   bool debug = true;
   DBGMSF(debug, "trace_flags=0x%04x\n", trace_flags);

   trace_levels = trace_flags;
}


/** Adds to the groups to be traced.
 *
 * @param trace_flags bit flags indicating groups to trace
 *
 * @ingroup dbgtrace
 */
void add_trace_groups(DDCA_Trace_Group trace_flags) {
   bool debug = false;
   DBGMSF(debug, "trace_flags=0x%04x\n", trace_flags);

   trace_levels |= trace_flags;
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
   bool debug = false;
   if (debug)
      printf("(%s) Starting. funcname=|%s|\n", __func__, funcname);

   if (!traced_function_table)
      traced_function_table = g_ptr_array_new();
   // n. g_ptr_array_find_with_equal_func() requires glib 2.54
   bool missing = (gaux_string_ptr_array_find(traced_function_table, funcname) < 0);
   if (missing)
      g_ptr_array_add(traced_function_table, g_strdup(funcname));

   if (debug)
      printf("(%s) Done. funcname=|%s|, missing=%s\n",
             __func__, funcname, SBOOL(missing));
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
   bool debug = false;
   if (debug)
      printf("(%s) Starting. filename = |%s| \n", __func__, filename);

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

   bool missing = (gaux_string_ptr_array_find(traced_file_table, bname) < 0);
   if (missing)
      g_ptr_array_add(traced_file_table, bname);
   else
      free(bname);
   if (debug)
      printf("(%s) Done. filename=|%s|, bname=|%s|, missing=%s\n",
             __func__, filename, bname, SBOOL(missing));
}


/** Checks if a function is being traced.
 *
 *  @param funcname function name
 *  @return **true** if the function is being traced, **false** if not
 */
bool is_traced_function(const char * funcname) {
   bool result = (traced_function_table && gaux_string_ptr_array_find(traced_function_table, funcname) >= 0);
   // printf("(%s) funcname=|%s|, returning: %s\n", __func__, funcname, SBOOL(result2));
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
      // printf("(%s) filename=|%s|, bname=|%s|, returning: %s\n", __func__, filename, bname, SBOOL(result));
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


#ifdef UNUSED
static char * trace_destination = NULL;

void show_trace_destination() {
   print_simple_title_value(SHOW_REPORTING_TITLE_START, "Trace destination:",
         SHOW_REPORTING_MIN_TITLE_SIZE,
         (trace_destination) ? trace_destination : "sysout");
}
#endif

#ifdef UNUSED
void set_libddcutil_output_destination(const char * filename, const char * trace_unit) {
   bool debug = false;
   if (debug)
      printf("(%s) filename = %s, trace_unit = %s\n", __func__, filename, trace_unit);
   if (filename) {
      trace_destination = strdup(filename);

      FILE * f = fopen(filename, "a");
      if (f) {
         time_t trace_start_time = time(NULL);
         char * trace_start_time_s = asctime(localtime(&trace_start_time));
         if (trace_start_time_s[strlen(trace_start_time_s)-1] == 0x0a)
              trace_start_time_s[strlen(trace_start_time_s)-1] = 0;
         fprintf(f, "%s tracing started %s\n", trace_unit, trace_start_time_s);

         fclose(f);
         if (debug)
            fprintf(stdout, "Writing %s trace output to %s\n", trace_unit, filename);
      }
      else {
         fprintf(stderr, "Unable to write %s trace output to %s: %s\n", trace_unit,
                         filename, strerror(errno));
      }
   }
}
#endif

#ifdef FUTURE
void init_syslog(const char * ddcutil_component) {
   openlog(ddcutil_component,
         LOG_CONS | LOG_PID,
         LOG_USER);

void close_syslog() {
   closelog();
}
#endif


/** Checks if a tracing is to be performed.
 *
 * Tracing is enabled if any of the following tests pass:
 * - trace group
 * - file name
 * - function name
 *
 * @param trace_group group to check
 * @param filename    file from which check is occurring
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
   bool debug = false;
   if (debug)
      printf("(%s) Starting. trace_group=0x%04x, filename=%s, funcname=%s\n",
              __func__, trace_group, filename, funcname);

   bool result =  (trace_group == DDCA_TRC_ALL) || (trace_levels & trace_group); // is trace_group being traced?

   result = result || is_traced_function(funcname) || is_traced_file(filename);

   if (debug)
      printf("(%s) Done.     trace_group=0x%04x, filename=%s, funcname=%s, trace_levels=0x%04x, returning %d\n",
              __func__, trace_group, filename, funcname, trace_levels, result);
   return result;
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
         // use dbgtrc() for consistent handling of timestamp and thread id prefixes
         dbgtrc(0xff, DBGTRC_OPTIONS_NONE, funcname, lineno, filename, "DDC: %s", buffer);
      }
      else {
         f0printf(fout(), "DDC: %s\n", buffer);

         // printf("trace_to_syslog = %s\n", sbool(trace_to_syslog));
         if (trace_to_syslog) {    // HACK
            syslog(LOG_INFO, "%s", buffer);
         }
      }
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
                              SBOOL(report_ddc_errors));
}

void show_ddcutil_version() {
   print_simple_title_value(SHOW_REPORTING_TITLE_START,
                              "ddcutil version: ",
                              SHOW_REPORTING_MIN_TITLE_SIZE,
                              get_full_ddcutil_version());
}


/** Reports the current trace settings.
 *
 *  \param depth  logical indentation depth
 */
void report_tracing(int depth) {
   int d1 = depth+1;
   rpt_label(depth, "Trace Options:");

#ifdef UNUSED
   show_trace_destination();
#endif

   char * buf = vnt_interpret_flags(trace_levels, trace_group_table, true /* use title */, ", ");
   rpt_vstring(d1, "Trace groups active:     %s", (buf && strlen(buf)>0) ? buf : "none");
   free(buf);

   buf = get_traced_functions_as_joined_string();
   rpt_vstring(d1, "Traced functions:        %s", (buf && strlen(buf)>0) ? buf : "none");
   free(buf);

   buf = get_traced_files_as_joined_string();
   rpt_vstring(d1, "Traced files:            %s", (buf && strlen(buf)>0) ? buf : "none");
   free(buf);

   rpt_vstring(d1, "Trace to syslog:         %s", SBOOL(trace_to_syslog));
   rpt_nl();
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


   // f0puts("", fout());
}


/** Returns the wall time as a formatted string.
 *
 *  The string is built in a thread specific private buffer.  The returned
 *  string is valid until the next call of this function in the same thread.
 *
 *  @return formatted wall time
 */
char * formatted_wall_time() {
   static GPrivate  formatted_wall_time_key = G_PRIVATE_INIT(g_free);
   char * time_buf = get_thread_fixed_buffer(&formatted_wall_time_key, 40);

   time_t epoch_seconds = time(NULL);
   struct tm broken_down_time;
   localtime_r(&epoch_seconds, &broken_down_time);

   strftime(time_buf, 40, "%b %d %T", &broken_down_time);

   // printf("(%s) |%s|\n", __func__, time_buf);
   return time_buf;
}



//
// Issue messages of various types
//

bool trace_to_syslog;


/** Issues an error message.
 *  The message is written to the current FERR device and to the system log.
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
      g_snprintf(buf2, 250, "(%s) %s", funcname, buffer);
      f0puts(buf2, ferr());
      f0putc('\n', ferr());
      fflush(ferr());
      va_end(args);

      syslog(LOG_ERR, "%s", buf2);
}


#ifdef OLD
/** Core function for emitting debug or trace messages.
 *  Normally wrapped in a DBGMSG or TRCMSG macro to simplify calling.
 *
 *  The message is output if any of the following are true:
 *  - the trace_group specified is currently active
 *  - the value is trace group is 0xff
 *  - funcname is the name of a function being traced
 *  - filename is the name of a file being traced
 *
 *  The message is output to the current FERR device and optionally,
 *  depending on the syslog setting, to the system log.
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
bool dbgtrc_old(
        DDCA_Trace_Group  trace_group,
        const char *      funcname,
        const int         lineno,
        const char *      filename,
        char *            format,
        ...)
{
   bool debug = false;
   if (debug)
      printf("(dbgtrc) Starting. trace_group = 0x%04x, funcname=%s"
             " filename=%s, lineno=%d, thread=%ld, fout() %s sysout\n",
                       trace_group, funcname, filename, lineno, syscall(SYS_gettid),
                       (fout() == stdout) ? "==" : "!=");

   bool msg_emitted = false;
   if ( is_tracing(trace_group, filename, funcname) ) {
      va_list(args);
      va_start(args, format);
      char * buffer = g_strdup_vprintf(format, args);
      va_end(args);

      char  elapsed_prefix[20]  = "";
      char  walltime_prefix[20] = "";
      if (dbgtrc_show_time)
         g_snprintf(elapsed_prefix, 20, "[%s]", formatted_elapsed_time());
      if (dbgtrc_show_wall_time)
         g_snprintf(walltime_prefix, 20, "[%s]", formatted_wall_time());

      char thread_prefix[15] = "";
      if (dbgtrc_show_thread_id) {
#ifdef TARGET_BSD
      int tid = pthread_getthreadid_np();
#else
         pid_t tid = syscall(SYS_gettid);
#endif
         snprintf(thread_prefix, 15, "[%7jd]", (intmax_t) tid);  // is this proper format for pid_t
      }

      char * buf2 = g_strdup_printf("%s%s%s(%-32s) %s",
                                    thread_prefix, walltime_prefix, elapsed_prefix, funcname, buffer);
#ifdef NO
      if (trace_destination) {
         FILE * f = fopen(trace_destination, "a");
         if (f) {
            int status = fputs(buf2, f);
            if (status < 0) {    // per doc it's -1 = EOF
               fprintf(stderr, "Error writing to %s: %s\n", trace_destination, strerror(errno));
               free(trace_destination);
               trace_destination = NULL;
            }
            else {
               fflush(f);
            }
            fclose(f);
         }
         else {
            fprintf(stderr, "Error opening %s: %s\n", trace_destination, strerror(errno));
            trace_destination = NULL;
         }
      }
      if (!trace_destination) {
         f0puts(buf2, fout());    // no automatic terminating null
         fflush(fout());
      }

      free(buffer);
      free(buf2);
      msg_emitted = true;
   }
#endif

      if (trace_to_syslog) {
         syslog(LOG_INFO, "%s", buf2);
      }

      f0puts(buf2, fout());
      f0putc('\n', fout());
      fflush(fout());
      free(buffer);
      free(buf2);
      msg_emitted = true;
   }

   return msg_emitted;
}
#endif


/** Core function for emitting debug or trace messages.
 *  Normally wrapped in a DBGMSG or TRCMSG macro to simplify calling.
 *
 *  The message is output if any of the following are true:
 *  - the trace_group specified is currently active
 *  - the value is trace group is 0xff
 *  - funcname is the name of a function being traced
 *  - filename is the name of a file being traced
 *
 *  The message is output to the current FERR device and optionally,
 *  depending on the syslog setting, to the system log.
 *
 *  @param trace_group   trace group of caller, 0xff to always output
 *  @param options       execution option flags
 *  @param funcname      function name of caller
 *  @param lineno        line number in caller
 *  @param filename      file name of caller
 *  @param pre_prefix
 *  @param format        format string for message
 *  @param ap            arguments for format string
 *
 *  @return **true** if message was output, **false** if not
 */
bool vdbgtrc(
        DDCA_Trace_Group  trace_group,
        Dbgtrc_Options    options,
        const char *      funcname,
        const int         lineno,
        const char *      filename,
        const char *      pre_prefix,
        char *            format,
        va_list           ap)
{
   bool debug = false;
   if (debug) {
      printf("(vdbgtrc) Starting. trace_group = 0x%04x, funcname=%s"
             " filename=%s, lineno=%d, thread=%ld, fout() %s sysout, pre_prefix=|%s|, format=|%s|\n",
                       trace_group, funcname, filename, lineno, get_thread_id(),
                       (fout() == stdout) ? "==" : "!=",
                       pre_prefix, format);
   }

   bool msg_emitted = false;
   if ( is_tracing(trace_group, filename, funcname) || (options & DBGTRC_OPTIONS_SYSLOG) ) {
      Thread_Output_Settings * thread_settings = get_thread_settings();

      char * buffer = g_strdup_vprintf(format, ap);
      if (debug)
         printf("(%s) buffer=%p->|%s|\n", __func__, buffer, buffer);

      if (!pre_prefix)
         pre_prefix="";
      if (debug)
         printf("(%s) pre_prefix=%p->|%s|\n", __func__, pre_prefix, pre_prefix);

      char  elapsed_prefix[20]  = "";
      char  walltime_prefix[20] = "";
      if (dbgtrc_show_time)
         g_snprintf(elapsed_prefix, 20, "[%s]", formatted_elapsed_time());
      if (dbgtrc_show_wall_time)
         g_snprintf(walltime_prefix, 20, "[%s]", formatted_wall_time());

      char thread_prefix[15] = "";
      if (dbgtrc_show_thread_id) {
         // intmax_t tid = get_thread_id();
         // assert(tid == thread_settings->tid);
         snprintf(thread_prefix, 15, "[%7jd]", thread_settings->tid);  // is this proper format for pid_t
      }

      char * buf2 = g_strdup_printf("%s%s%s(%-30s) %s%s",
                       thread_prefix, walltime_prefix, elapsed_prefix, funcname,
                       pre_prefix, buffer);
      char * syslog_buf = g_strdup_printf("%s(%-30s) %s%s",
            elapsed_prefix, funcname,
            pre_prefix, buffer);
      if (debug)
         printf("(%s) buf2=%p->|%s|\n", __func__, buf2, buf2);
#ifdef NO
      if (trace_destination) {
         FILE * f = fopen(trace_destination, "a");
         if (f) {
            int status = fputs(buf2, f);
            if (status < 0) {    // per doc it's -1 = EOF
               fprintf(stderr, "Error writing to %s: %s\n", trace_destination, strerror(errno));
               free(trace_destination);
               trace_destination = NULL;
            }
            else {
               fflush(f);
            }
            fclose(f);
         }
         else {
            fprintf(stderr, "Error opening %s: %s\n", trace_destination, strerror(errno));
            trace_destination = NULL;
         }
      }
      if (!trace_destination) {
         f0puts(buf2, fout());    // no automatic terminating null
         fflush(fout());
      }

      free(pre_prefix_buffer);
      free(buf2);
      msg_emitted = true;
   }
#endif

      if (trace_to_syslog || (options & DBGTRC_OPTIONS_SYSLOG)) {
         syslog(LOG_INFO, "%s", syslog_buf);
      }

      // assert(fout() == thread_settings->fout);
      if (is_tracing(trace_group, filename, funcname)) {
         f0puts(buf2, thread_settings->fout);
         f0putc('\n', thread_settings->fout);
         fflush(fout());
      }
      free(buffer);
      free(buf2);
      free(syslog_buf);
      msg_emitted = true;
   }

   if (debug)
      printf("(%s) Done.   Returning %s\n", __func__, sbool(msg_emitted));
   return msg_emitted;
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
 *  The message is output to the current FERR device and optionally,
 *  depending on the syslog setting, to the system log.
 *
 *  @param trace_group   trace group of caller, DDCA_TRC_ALL = 0xffff to always output
 *  @param options       execution options
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
        Dbgtrc_Options    options,
        const char *      funcname,
        const int         lineno,
        const char *      filename,
        char *            format,
        ...)
{
   bool debug = false;
   if (debug)
      printf("(dbgtrc) Starting. trace_group = 0x%04x, funcname=%s"
             " filename=%s, lineno=%d, thread=%ld, fout() %s sysout\n",
                       trace_group, funcname, filename, lineno, get_thread_id(),
                       (fout() == stdout) ? "==" : "!=");

   bool msg_emitted = false;
   if ( is_tracing(trace_group, filename, funcname) ) {
      va_list(args);
      va_start(args, format);
      // if (debug)
      //    printf("(%s) &args=%p, args=%p\n", __func__, &args, args);
      msg_emitted = vdbgtrc(trace_group, options, funcname, lineno, filename, "", format, args);
      va_end(args);
   }

   if (debug)
      printf("(%s) Done.     Returning %s\n", __func__, sbool(msg_emitted));
   return msg_emitted;
}


bool dbgtrc_returning(
        DDCA_Trace_Group  trace_group,
        Dbgtrc_Options    options,
        const char *      funcname,
        const int         lineno,
        const char *      filename,
        int               rc,
        char *            format,
        ...)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. trace_group = 0x%04x, funcname=%s"
             " filename=%s, lineno=%d, thread=%ld, fout() %s sysout, rc=%d, format=|%s|\n",
                       __func__,
                       trace_group, funcname, filename, lineno, get_thread_id(),
                       (fout() == stdout) ? "==" : "!=",
                       rc, format);

   bool msg_emitted = false;
   if ( is_tracing(trace_group, filename, funcname) ) {
      char pre_prefix[60];
      g_snprintf(pre_prefix, 60, "Done      Returning: %s. ", psc_name_code(rc));
      if (debug)
         printf("(%s) pre_prefix=|%s|\n", __func__, pre_prefix);

      va_list(args);
      va_start(args, format);
      if (debug)
         printf("(%s) &args=%p, args=%p\n", __func__, (void*)&args, (void*)args);
      msg_emitted = vdbgtrc(trace_group, options, funcname, lineno, filename, pre_prefix, format, args);
      va_end(args);
   }
   if (debug)
      printf("(%s) Done.     Returning %s\n", __func__, sbool(msg_emitted));
   return msg_emitted;
}


bool dbgtrc_returning_errinfo(
        DDCA_Trace_Group  trace_group,
        Dbgtrc_Options    options,
        const char *      funcname,
        const int         lineno,
        const char *      filename,
        Error_Info *      errs,
        char *            format,
        ...)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. trace_group = 0x%04x, funcname=%s"
             " filename=%s, lineno=%d, thread=%ld, fout() %s sysout, errs=%p, format=|%s|\n",
                       __func__,
                       trace_group, funcname, filename, lineno, get_thread_id(),
                       (fout() == stdout) ? "==" : "!=",
                       (void*)errs, format);

   bool msg_emitted = false;
   if ( is_tracing(trace_group, filename, funcname) ) {
      char * pre_prefix = g_strdup_printf("Done      Returning: %s. ", errinfo_summary(errs));
      if (debug)
         printf("(%s) pre_prefix=|%s|\n", __func__, pre_prefix);

      va_list(args);
      va_start(args, format);
      if (debug)
         printf("(%s) &args=%p, args=%p\n", __func__, (void*)&args, (void*)args);
      msg_emitted = vdbgtrc(trace_group, options, funcname, lineno, filename, pre_prefix, format, args);
      va_end(args);
      g_free(pre_prefix);
   }

   if (debug)
      printf("(%s) Done.     Returning %s\n", __func__, sbool(msg_emitted));
   return msg_emitted;
}


bool dbgtrc_returning_expression(
        DDCA_Trace_Group  trace_group,
        Dbgtrc_Options    options,
        const char *      funcname,
        const int         lineno,
        const char *      filename,
        const char *      retval,
        char *            format,
        ...)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. trace_group = 0x%04x, funcname=%s"
             " filename=%s, lineno=%d, thread=%ld, fout() %s sysout, retval=%s, format=|%s|\n",
                       __func__,
                       trace_group, funcname, filename, lineno, get_thread_id(),
                       (fout() == stdout) ? "==" : "!=",
                       retval, format);

   bool msg_emitted = false;
   if ( is_tracing(trace_group, filename, funcname) ) {
      char pre_prefix[60];
      g_snprintf(pre_prefix, 60, "Done      Returning: %s. ", retval);
      if (debug)
         printf("(%s) pre_prefix=|%s|\n", __func__, pre_prefix);

      va_list(args);
      va_start(args, format);
      if (debug)
         printf("(%s) &args=%p, args=%p\n", __func__, (void*)&args, (void*)args);
      msg_emitted = vdbgtrc(trace_group, options, funcname, lineno, filename, pre_prefix, format, args);
      va_end(args);
   }
   if (debug)
      printf("(%s) Done.     Returning %s\n", __func__, sbool(msg_emitted));
   return msg_emitted;
}


//
// Standardized handling of exceptional conditions, including
// error messages and possible program termination.
//

/** Reports an IOCTL error.
 *  The message is written to the current **FERR** device and to the system log.
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
   char buffer[200];
   g_snprintf(buffer, 200, "(%s) Error in ioctl(%s), errno=%s",
                           funcname, ioctl_name, linux_errno_desc(errnum) );
   f0puts(buffer, ferr());
   f0putc('\n',   ferr());
   fflush(ferr());

   syslog(LOG_ERR, "%s", buffer);

   errno = errsv;
}


/** Called when a condition that should be impossible has been detected.
 *  Issues messages to the current **FERR** device and the system log.
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
   snprintf(buf2, 250, "Program logic error in function %s at line %d in file %s:",
                       funcname, lineno, fn);

   // don't combine into 1 line, might be very long.  just output 2 lines:
   FILE * f = ferr();
   f0printf(f, "%s\n", buf2);
   f0printf(f, "%s\n", buffer);
   fflush(f);

   syslog(LOG_ERR, "%s", buf2);
   syslog(LOG_ERR, "%s", buffer);
}


void init_core() {
}
