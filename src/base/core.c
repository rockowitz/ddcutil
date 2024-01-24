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

// Copyright (C) 2014-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#define _GNU_SOURCE    // for syscall(), localtime_r()

//* \cond */
#include <glib-2.0/glib.h>
#include <errno.h>
#include <rtti.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <threads.h>  // requires glibc 2.28, apparently unused

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
#include "util/error_info.h"
#include "util/file_util.h"
#include "util/glib_util.h"
#include "util/glib_string_util.h"
#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/timestamp.h"

#include "base/build_info.h"
#include "base/core_per_thread_settings.h"
#include "base/ddc_errno.h"
#include "base/linux_errno.h"
#include "base/trace_control.h"

#include "base/core.h"

bool tracing_initialized = false;

//
// Standard call options
//

Value_Name_Table callopt_bitname_table2 = {
      VN(CALLOPT_ERR_MSG),
 //   VN(CALLOPT_ERR_ABORT),
      VN(CALLOPT_RDONLY),
      VN(CALLOPT_WARN_FINDEX),
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
print_simple_title_value(int          offset_start_to_title,
                         const char * title,
                         int          offset_title_start_to_value,
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

bool dbgtrc_show_time      =  false;  ///< include elapsed time in debug/trace output
bool dbgtrc_show_wall_time =  false;  ///< include wall time in debug/trace output
bool dbgtrc_show_thread_id =  false;  ///< include thread id in debug/trace output
bool dbgtrc_show_process_id = false;  ///< include process id in debug/trace output
bool dbgtrc_trace_to_syslog_only = false; ///< send trace output only to system log


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


//
// Error_Info reporting
//

/** If true, report #Error_Info instances before they are freed. */
bool report_freed_exceptions = false;


//
// Report DDC data errors
//

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
        report_ddc_errors);
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
         if (test_emit_syslog(DDCA_SYSLOG_WARNING)) {
            syslog(LOG_WARNING, "%s", buffer);
         }
      }
      fflush(fout());
      va_end(args);
   }
   return result;
}


bool logable_msg(DDCA_Syslog_Level log_level,
            const char * funcname,
            const int    lineno,
            const char * filename,
            char *       format,
            ...)
{
   bool result = true;
   // char buffer[500];
   va_list(args);
   va_start(args, format);
   char * buffer = g_strdup_vprintf(format, args);
   // vsnprintf(buffer, 500, format, args);
   f0printf(fout(), "%s\n", buffer);
   if (test_emit_syslog(log_level)) {
      int importance = syslog_importance_from_ddcutil_syslog_level(log_level);
      syslog(importance, "%s", buffer);
   }
   fflush(fout());
   va_end(args);
   free(buffer);
   return result;
}


/** Tells whether DDC data errors are reported.
 *  Output is written to the current **FOUT** device.
 */
static void show_ddcmsg() {
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


/** Reports output levels for:
 *   - general output level (terse, verbose, etc)
 *   - DDC data errors
 *
 * Output is written to the current **FOUT** device.
 */
void show_reporting() {
   show_output_level();
   show_ddcmsg();
}


/** Returns the wall time as a formatted string.
 *
 *  The string is built in a thread specific private buffer.  The returned
 *  string is valid until the next call of this function in the same thread.
 *
 *  @return formatted wall time
 */
static char * formatted_wall_time() {
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

#define MAX_TRACE_CALLSTACK_CALL_DEPTH 100

// trace_callstack is per thread
__thread  int    trace_api_call_depth = 0;
__thread  unsigned int    trace_callstack_call_depth = 0;



/** Checks if tracing is to be performed.
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
   bool debug = false;  //str_starts_with(funcname, "ddca_");
   if (debug)
      printf("(%s) Starting. trace_group=0x%04x, filename=%s, funcname=%s\n",
              __func__, trace_group, filename, funcname);
   bool result = false;
// #ifdef ENABLE_TRACE
   result =  (trace_group == DDCA_TRC_ALL) || (trace_levels & trace_group); // is trace_group being traced?

   result = result || is_traced_function(funcname) || is_traced_file(filename) || trace_api_call_depth > 0;
// #endif
   if (debug)
      printf("(%s) Done.     trace_group=0x%04x, filename=%s, funcname=%s, trace_levels=0x%04x, returning %d\n",
              __func__, trace_group, filename, funcname, trace_levels, result);
   return result;
}


#ifdef UNUSED

 #define INIT_CALLSTACK() \
 if (!trace_callstack) { \
    trace_callstack = g_ptr_array_sized_new(100); \
    g_ptr_array_set_free_func(trace_callstack, g_free); \
 }



static void report_callstack() {
   // INIT_CALLSTACK();
   printf("Current callstack, trace_callstack_call_depth=%d\n",
          trace_callstack_call_depth);
   for (int ndx = 0; ndx < trace_callstack_call_depth; ndx++) {
      printf("   trace_callstack[%d] = %s\n", ndx, trace_callstack[ndx]);
   }
}


static void push_callstack(const char * funcname) {
   // INIT_CALLSTACK();
   bool debug = true;
   if (debug)
      printf("(%s) Starting. funcname=%s, trace_callstack_call_depth=%d\n",
            __func__, funcname, trace_callstack_call_depth);
   assert(trace_callstack_call_depth < (MAX_TRACE_CALLSTACK_CALL_DEPTH-1));
   trace_callstack[trace_callstack_call_depth++] = strdup(funcname);
   if (debug)
      printf("(%s) Done.  New trace_callstack_call_depth = %d\n", __func__, trace_callstack_call_depth);
}


static void pop_callstack(const char * funcname) {
   // INIT_CALLSTACK();
   bool debug = true;
   if (debug)
      printf("(%s) Starting. funcname=%s, trace_callstack_call_depth=%d\n",
            __func__, funcname, trace_callstack_call_depth);

   if (trace_callstack_call_depth == 0) {
      printf("(%s) ======> Error. Popping %s off of empty call stack\n", __func__, funcname);
      assert(false);
   }
   else {
      int last = trace_callstack->len - 1;
      char * popped = g_ptr_array_remove_index(trace_callstack, last);
      if (!(streq(funcname, popped))) {
         printf("(%s) ======> Error. Popped %s, expected %s\n", __func__, popped, funcname);
         report_callstack();
         show_backtrace(2);
         assert(streq(funcname, popped));
      }
      if (debug)
         printf("(%s) Done.    Popped: %s, new callstack->len=%d\n", __func__, popped, trace_callstack->len);
      free(popped);
   }
}
#endif


/** Core function for emitting debug and trace messages.
 *  Used by the dbgtrc*() function variants.
 *
 *  The message is output if any of the following are true:
 *  - the trace_group specified is currently active
 *  - the value is trace group is 0xff
 *  - funcname is the name of a function being traced
 *  - filename is the name of a file being traced
 *  - api stack call depth > 0
 *
 *  The message is written to the fout() or ferr() device for the current thread
 *  and optionally, depending on the syslog setting, to the system log.
 *
 *  @param trace_group   trace group of caller, 0xff to always output
 *  @param options       execution option flags
 *  @param funcname      function name of caller
 *  @param lineno        line number in caller
 *  @param filename      file name of caller
 *  @param retval_info   return value description
 *  @param format        format string for message
 *  @param ap            arguments for format string
 *
 *  @return **true** if message was output, **false** if not
 */
static bool vdbgtrc(
        DDCA_Trace_Group  trace_group,
        Dbgtrc_Options    options,
        const char *      funcname,
        const int         lineno,
        const char *      filename,
        const char *      retval_info,
        char *            format,
        va_list           ap)
{
   bool debug = false;
   if (debug) {
      printf("(vdbgtrc) Starting. trace_group=0x%04x, options=0x%02x, funcname=%s"
             " filename=%s, lineno=%d, thread=%ld, fout() %s sysout, pre_prefix=|%s|, format=|%s|\n",
                       trace_group, options, funcname, filename, lineno, get_thread_id(),
                       (fout() == stdout) ? "==" : "!=",
                       retval_info, format);
      printf("(vdbgtrc) trace_api_call_depth=%d\n", trace_api_call_depth);
   }

   bool msg_emitted = false;

   if (trace_api_call_depth > 0 || trace_callstack_call_depth > 0)
      trace_group = DDCA_TRC_ALL;
   if (debug)
      printf("(%s) Adjusted trace_group == 0x%02x\n", __func__, trace_group);

   bool perform_emit = true;
// #ifndef ENABLE_TRACE
//    if (!(options & DBGTRC_OPTIONS_SEVERE))
//       perform_emit = false;
// #endif

   if (perform_emit) {
      Thread_Output_Settings * thread_settings = get_thread_settings();
      // n. trace_group == DDCA_TRC_ALL for SEVEREMSG() or API call tracing
      if ( is_tracing(trace_group, filename, funcname)  ) {
         char * base_msg = g_strdup_vprintf(format, ap);
         if (debug) {
            printf("(%s) base_msg=%p->|%s|\n", __func__, base_msg, base_msg);
            printf("(%s) retval_info=%p->|%s|\n", __func__, retval_info, retval_info);
         }
         char elapsed_prefix[20]  = "";
         char walltime_prefix[20] = "";
         char thread_prefix[15]   = "";
         char process_prefix[15]  = "";
         if (dbgtrc_show_time      && !(options & DBGTRC_OPTIONS_SEVERE))
            g_snprintf(elapsed_prefix, 20, "[%s]", formatted_elapsed_time_t(4));
         if (dbgtrc_show_wall_time && !(options & DBGTRC_OPTIONS_SEVERE))
            g_snprintf(walltime_prefix, 20, "[%s]", formatted_wall_time());
         if (dbgtrc_show_thread_id && !(options & DBGTRC_OPTIONS_SEVERE) ) {
            // intmax_t tid = get_thread_id();
            // assert(tid == thread_settings->tid);
            snprintf(thread_prefix, 15, "[%7jd]", thread_settings->tid);
         }
         if (dbgtrc_show_process_id && !(options & DBGTRC_OPTIONS_SEVERE) ) {
            intmax_t pid = get_process_id();
            // assert(pid == thread_settings->pid);
            snprintf(process_prefix, 15, "{%7jd}", pid);
         }
         char * decorated_msg = (options & DBGTRC_OPTIONS_SEVERE)
                   ? g_strdup_printf("%s%s",
                          retval_info, base_msg)
                   : g_strdup_printf("%s%s%s%s(%-30s) %s%s",
                          process_prefix, thread_prefix, walltime_prefix, elapsed_prefix, funcname,
                          retval_info, base_msg);
         if (debug)
            printf("(%s) decorated_msg=%p->|%s|\n", __func__, decorated_msg, decorated_msg);



#ifdef NO
         if (trace_destination) {
            FILE * f = fopen(trace_destination, "a");
            if (f) {
               int status = fputs(decorated_msg, f);
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
            f0puts(decorated_msg, fout());    // no automatic terminating null
            fflush(fout());
         }

         free(pre_prefix_buffer);
         free(decorated_msg);
         msg_emitted = true;
#endif

         // if (trace_to_syslog || (options & DBGTRC_OPTIONS_SYSLOG)) {
         if (test_emit_syslog(DDCA_SYSLOG_DEBUG) || dbgtrc_trace_to_syslog_only) {
            char * syslog_msg = g_strdup_printf("%s(%-30s) %s%s",
                                     elapsed_prefix, funcname, retval_info, base_msg);
            syslog(LOG_DEBUG, "%s", syslog_msg);
            free(syslog_msg);
         }
         else if ( (options & DBGTRC_OPTIONS_SEVERE) && test_emit_syslog(DDCA_SYSLOG_ERROR)) {
            char * syslog_msg = g_strdup_printf("%s(%-30s) %s%s",
                                     elapsed_prefix, funcname, retval_info, base_msg);
            syslog(LOG_ERR, "%s", syslog_msg);
            free(syslog_msg);
         }

         if (!dbgtrc_trace_to_syslog_only) {
            FILE * where = (options & DBGTRC_OPTIONS_SEVERE)
                              ? thread_settings->ferr
                              : thread_settings->fout;
            f0printf(where, "%s\n", decorated_msg);
            // f0puts(decorated_msg, where);
            // f0putc('\n', where);
            fflush(where);
         }

         free(decorated_msg);
         free(base_msg);
         msg_emitted = true;
      }
   }

   if (debug)
      printf("(%s) Done.   Returning %s\n", __func__, sbool(msg_emitted));
   return msg_emitted;
}


bool check_callstack(Dbgtrc_Options options, const char * funcname) {
   bool debug = false;
   // debug = debug || trace_callstack_call_depth > 0 || is_traced_callstack_call(funcname);
   if (debug)
      printf("\n(%s) Starting. options=0x%04x, funcname=%s, trace_callstack_call_depth=%d\n",
            __func__, options, funcname, trace_callstack_call_depth);

   if (options & DBGTRC_OPTIONS_STARTING) {
      if (trace_callstack_call_depth > 0) {
         trace_callstack_call_depth++;
      }
      else {
         if (is_traced_callstack_call(funcname)) {
            trace_callstack_call_depth = 1;
         }
      }
      if (debug)
         printf("(%s(           trace_callstack_call_depth=%d\n", __func__, trace_callstack_call_depth);
   }

   if ((options & DBGTRC_OPTIONS_DONE) && trace_callstack_call_depth > 0) {
      trace_callstack_call_depth--;
   }

   if (debug)
      printf("(%s) Done.     trace_callstack_call_depth=%d, returning %s\n",
            __func__, trace_callstack_call_depth, sbool(trace_callstack_call_depth > 0));
   return trace_callstack_call_depth > 0;
}


/** Basic function for emitting debug or trace messages.
 *  Normally wrapped in a DBGMSG or DBGTRC macro to simplify calling.
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
      printf("(dbgtrc) Starting. trace_group=0x%04x, options=0x%02x, funcname=%s"
             " filename=%s, lineno=%d, thread=%ld, trace_callstack_call_depth=%d, fout() %s sysout\n",
                       trace_group, options, funcname, filename, lineno, get_thread_id(), trace_callstack_call_depth,
                       (fout() == stdout) ? "==" : "!=");

   bool msg_emitted = false;
   bool in_callstack = check_callstack(options, funcname);
   if ( in_callstack || is_tracing(trace_group, filename, funcname) ) {
      va_list(args);
      va_start(args, format);
      // if (debug)
      //    printf("(%s) &args=%p, args=%p\n", __func__, &args, args);
      msg_emitted = vdbgtrc(trace_group, options, funcname, lineno, filename, "", format, args);
      va_end(args);
   }

   if (debug)
      printf("(%s) Done.      trace_callstack_call_depth=%d, Returning %s\n", __func__, trace_callstack_call_depth, sbool(msg_emitted));
   return msg_emitted;
}


/** dbgtrc() variant that reports a numeric return code (normally of
 *  type #DDCA_Status), in a standardized form.
 */
bool dbgtrc_ret_ddcrc(
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
   bool in_callstack = check_callstack(options, funcname);
   if ( in_callstack || is_tracing(trace_group, filename, funcname) ) {
      char pre_prefix[60];
      g_snprintf(pre_prefix, 60, "Done      Returning: %s. ", psc_name_code(rc));
      if (debug)
         printf("(%s) pre_prefix=|%s|\n", __func__, pre_prefix);

      va_list(args);
      va_start(args, format);
      // arm7l, aarch64: "on  error: cannot convert to a pointer type"
      // if (debug)
      //    printf("(%s) &args=%p, args=%p\n", __func__, (void*)&args, (void*)args);
      msg_emitted = vdbgtrc(trace_group, options, funcname, lineno, filename, pre_prefix, format, args);
      va_end(args);
   }
   if (debug)
      printf("(%s) Done.     Returning %s\n", __func__, sbool(msg_emitted));
   return msg_emitted;
}

#ifdef UNTESTED
// unnecessary, use dbgtrc_returning_expression()
bool dbgtrc_ret_bool(
        DDCA_Trace_Group  trace_group,
        Dbgtrc_Options    options,
        const char *      funcname,
        const int         lineno,
        const char *      filename,
        bool              result,
        char *            format,
        ...)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. trace_group = 0x%04x, funcname=%s"
             " filename=%s, lineno=%d, thread=%ld, fout() %s sysout, result=%s, format=|%s|\n",
                       __func__,
                       trace_group, funcname, filename, lineno, get_thread_id(),
                       (fout() == stdout) ? "==" : "!=",
                       sbool(result), format);

   bool msg_emitted = false;
   bool in_callstack = check_callstack(options, funcname);
   if ( in_callstack || is_tracing(trace_group, filename, funcname) ) {
      char pre_prefix[60];
      g_snprintf(pre_prefix, 60, "Done      Returning: %s. ", sbool(result));
      if (debug)
         printf("(%s) pre_prefix=|%s|\n", __func__, pre_prefix);

      va_list(args);
      va_start(args, format);
      // arm7l, aarch64: "on  error: cannot convert to a pointer type"
      // if (debug)
      //    printf("(%s) &args=%p, args=%p\n", __func__, (void*)&args, (void*)args);
      msg_emitted = vdbgtrc(trace_group, options, funcname, lineno, filename, pre_prefix, format, args);
      va_end(args);
   }
   if (debug)
      printf("(%s) Done.     Returning %s\n", __func__, sbool(msg_emitted));
   return msg_emitted;
}
#endif


/** dbgtrc() variant that reports a return code of type #Error_Info in a
 *  standardized form.
 */
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
   bool in_callstack = check_callstack(options, funcname);
   if ( in_callstack || is_tracing(trace_group, filename, funcname) ) {
      char * pre_prefix = g_strdup_printf("Done      Returning: %s. ", errinfo_summary(errs));
      if (debug)
         printf("(%s) pre_prefix=|%s|\n", __func__, pre_prefix);

      va_list(args);
      va_start(args, format);
      // arm7l, aarch64: "on  error: cannot convert to a pointer type"
      // if (debug)
      //    printf("(%s) &args=%p, args=%p\n", __func__, (void*)&args, (void*)args);
      msg_emitted = vdbgtrc(trace_group, options, funcname, lineno, filename, pre_prefix, format, args);
      va_end(args);
      g_free(pre_prefix);
   }

   if (debug)
      printf("(%s) Done.     Returning %s\n", __func__, sbool(msg_emitted));
   return msg_emitted;
}


/** dbgtrc() variant that reports a return value specified as a string.
 */
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
   bool in_callstack = check_callstack(options, funcname);
   if ( in_callstack || is_tracing(trace_group, filename, funcname) ) {
      char * pre_prefix = g_strdup_printf("Done      Returning: %s. ", retval);
      if (debug)
         printf("(%s) pre_prefix=|%s|\n", __func__, pre_prefix);

      va_list(args);
      va_start(args, format);
      // arm7l, aarch64: "on  error: cannot convert to a pointer type"
      // if (debug)
      //    printf("(%s) &args=%p, args=%p\n", __func__, (void*)&args, (void*)args);
      msg_emitted = vdbgtrc(trace_group, options, funcname, lineno, filename, pre_prefix, format, args);
      va_end(args);
      free(pre_prefix);
   }
   if (debug)
      printf("(%s) Done.     Returning %s\n", __func__, sbool(msg_emitted));
   return msg_emitted;
}


//
// Standardized handling of exceptional conditions, including
// error messages and possible program termination.
//

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

   SYSLOG2(DDCA_SYSLOG_ERROR, "%s", buf2);
   SYSLOG2(DDCA_SYSLOG_ERROR, "%s", buffer);
}


#ifdef UNUSED
void core_errmsg_emitter(
      GPtrArray*   errmsgs,
      GPtrArray *  errinfo_accum,
      bool         verbose,
      int          rc,
      const char * func,
      const char * msg, ...)
{
   char buffer[200];
   va_list(args);
   va_start(args, msg);
   vsnprintf(buffer, 100, msg, args);
   va_end(args);

   if (verbose || (!errmsgs && !errinfo_accum))
      fprintf(ferr(), "%s\n", buffer);
   if (errinfo_accum) {
      Error_Info * erec =  errinfo_new(rc, func, buffer);
      g_ptr_array_add(errinfo_accum, erec);
   }
   if (errmsgs) {
      g_ptr_array_add(errmsgs, g_strdup(buffer));
   }
}
#endif


//
// Use system log
//

DDCA_Syslog_Level syslog_level = DDCA_SYSLOG_NOT_SET;
bool enable_syslog = true;

Value_Name_Title_Table syslog_level_table = {
      VNT(DDCA_SYSLOG_DEBUG,   "DEBUG"),
      VNT(DDCA_SYSLOG_VERBOSE, "VERBOSE"),
      VNT(DDCA_SYSLOG_INFO,    "INFO"),
      VNT(DDCA_SYSLOG_NOTICE,  "NOTICE"),
      VNT(DDCA_SYSLOG_WARNING, "WARN"),
      VNT(DDCA_SYSLOG_ERROR,   "ERROR"),
      VNT(DDCA_SYSLOG_NEVER,   "NEVER"),
      VNT_END
};
const int syslog_level_ct = (ARRAY_SIZE(syslog_level_table)-1);
const char * valid_syslog_levels_string = "DEBUG, VERBOSE, INFO, NOTICE, WARN, ERROR, NEVER";


const char * syslog_level_name(DDCA_Syslog_Level level) {
   char * result = "DDCA_SYSLOG_NOT_SET";
   if (level != DDCA_SYSLOG_NOT_SET)
      result = vnt_name(syslog_level_table, level);
   return result;
}


DDCA_Syslog_Level syslog_level_name_to_value(const char * name) {
   return (DDCA_Syslog_Level) vnt_find_id(syslog_level_table,
                                          name,
                                          true,      // search title field
                                          true,      // ignore-case
                                          DDCA_SYSLOG_NOT_SET);
}


/** Given a message severity level, test whether it should be
 *  written to the system log.
 *
 *  @param  msg_level  severity of message
 *  @return true if msg should be written to system log, false if not
 */
bool test_emit_syslog(DDCA_Syslog_Level msg_level) {
   bool result =  (syslog_level != DDCA_SYSLOG_NOT_SET && syslog_level != DDCA_SYSLOG_NEVER &&
         msg_level <= syslog_level);
   return result;
}


/** Given a ddcutil severity level for messages written to the system log,
 *  returns the syslog priority level to be used in a syslog() call.
 *
 *  @param  level ddcutil severity level
 *  @return priority for syslog() call,
 *          -1 for msg that should never be output
 */
int syslog_importance_from_ddcutil_syslog_level(DDCA_Syslog_Level level) {
   int priority = -1;
   switch(level) {
   case DDCA_SYSLOG_NOT_SET: priority = -1;           break;
   case DDCA_SYSLOG_NEVER:   priority = -1;           break;
   case DDCA_SYSLOG_ERROR:   priority = LOG_ERR;      break;  // 3
   case DDCA_SYSLOG_WARNING: priority = LOG_WARNING;  break;  // 4
   case DDCA_SYSLOG_NOTICE:  priority = LOG_NOTICE;   break;  // 5
   case DDCA_SYSLOG_INFO:    priority = LOG_INFO;     break;  // 6
   case DDCA_SYSLOG_VERBOSE: priority = LOG_INFO;     break;  // 6
   case DDCA_SYSLOG_DEBUG:   priority = LOG_DEBUG;    break;  // 7
   }
   return priority;
}


//
// Output capture - convenience functions
//

typedef struct {
   FILE * in_memory_file;
   char * in_memory_bufstart; ;
   size_t in_memory_bufsize;
   DDCA_Capture_Option_Flags flags;
   bool   in_memory_capture_active;
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
start_capture(DDCA_Capture_Option_Flags flags) {
   In_Memory_File_Desc * fdesc = get_thread_capture_buf_desc();

   if (!fdesc->in_memory_file) {
      fdesc->in_memory_file = open_memstream(&fdesc->in_memory_bufstart, &fdesc->in_memory_bufsize);
   }
   set_fout(fdesc->in_memory_file);   // n. ddca_set_fout() is thread specific
   fdesc->flags = flags;
   if (flags & DDCA_CAPTURE_STDERR)
      set_ferr(fdesc->in_memory_file);
   fdesc->in_memory_capture_active = true;
   // printf("(%s) Done.\n", __func__);
}


char *
end_capture(void) {
   In_Memory_File_Desc * fdesc = get_thread_capture_buf_desc();
   assert(fdesc->in_memory_capture_active);

   char * result = "\0";
   // printf("(%s) Starting.\n", __func__);
   assert(fdesc->in_memory_file);
   if (fflush(fdesc->in_memory_file) < 0) {
      set_ferr_to_default();
      SEVEREMSG("flush() failed. errno=%d", errno);
      return g_strdup(result);
   }
   // n. open_memstream() maintains a null byte at end of buffer, not included in in_memory_bufsize
   result = g_strdup(fdesc->in_memory_bufstart);
   if (fclose(fdesc->in_memory_file) < 0) {
      set_ferr_to_default();
      SEVEREMSG("fclose() failed. errno=%d", errno);
      return result;
   }
   // free(fdesc->in_memory_file); // double free, fclose() frees in memory file
   fdesc->in_memory_file = NULL;
   set_fout_to_default();
   if (fdesc->flags & DDCA_CAPTURE_STDERR)
      set_ferr_to_default();
   fdesc->in_memory_capture_active = false;

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
int captured_size() {
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


/** Releases a #Error_Info instance, including all instances it points to.
 *  Optionally reports the instance before freeing it, taking into account
 *  syslog redirection.
 *
 *  \param  erec   pointer to #Error_Info instance,
 *                 do nothing if NULL
 *  \param  report if true, report the instance
 *  \param  func   name of calling function
 */
void
base_errinfo_free_with_report(
      Error_Info * erec,
      bool         report,
      const char * func)
{
   if (erec) {
      if (report || report_freed_exceptions) {
         if ( dbgtrc_trace_to_syslog_only) {
            GPtrArray * collector = g_ptr_array_new_with_free_func(g_free);
            rpt_vstring_collect(0, collector, "(%s) Freeing exception:", func);
            for (int ndx = 0; ndx < collector->len; ndx++) {
              syslog(LOG_NOTICE, "%s", (char*) g_ptr_array_index(collector, ndx));
            }
            g_ptr_array_free(collector, true);
         }
         else {
            rpt_vstring(0, "(%s) Freeing exception:", func);
            errinfo_report(erec, 1);
         }
      }
      errinfo_free(erec);
   }
}


void init_core() {
}
