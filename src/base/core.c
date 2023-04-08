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

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include <threads.h>

#ifdef TARGET_BSD
#include <pthread_np.h>
#else
#include <sys/types.h>
#include <sys/syscall.h>
#ifdef ENABLE_SYSLOG
#include <syslog.h>
#endif
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

bool dbgtrc_show_time      = false;   ///< include elapsed time in debug/trace output
bool dbgtrc_show_wall_time = false;   ///< include wall time in debug/trace output
bool dbgtrc_show_thread_id = false;   ///< include thread id in debug/trace output



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

#ifdef ENABLE_SYSLOG
         // printf("trace_to_syslog = %s\n", sbool(trace_to_syslog));
         if (trace_to_syslog) {    // HACK
            syslog(LOG_INFO, "%s", buffer);
         }
#endif
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
      printf("(vdbgtrc) Starting. trace_group=0x%04x, options=0x%o2x, funcname=%s"
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
         char  elapsed_prefix[20]  = "";
         char  walltime_prefix[20] = "";
         char thread_prefix[15] = "";
         if (dbgtrc_show_time      && !(options & DBGTRC_OPTIONS_SEVERE))
            g_snprintf(elapsed_prefix, 20, "[%s]", formatted_elapsed_time_t(4));
         if (dbgtrc_show_wall_time && !(options & DBGTRC_OPTIONS_SEVERE))
            g_snprintf(walltime_prefix, 20, "[%s]", formatted_wall_time());
         if (dbgtrc_show_thread_id && !(options & DBGTRC_OPTIONS_SEVERE) ) {
            // intmax_t tid = get_thread_id();
            // assert(tid == thread_settings->tid);
            snprintf(thread_prefix, 15, "[%7jd]", thread_settings->tid);
         }
         char * decorated_msg = (options & DBGTRC_OPTIONS_SEVERE)
                   ? g_strdup_printf("%s%s",
                          retval_info, base_msg)
                   : g_strdup_printf("%s%s%s(%-30s) %s%s",
                          thread_prefix, walltime_prefix, elapsed_prefix, funcname,
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

#ifdef ENABLE_SYSLOG
         if (trace_to_syslog || (options & DBGTRC_OPTIONS_SYSLOG)) {
            char * syslog_msg = g_strdup_printf("%s(%-30s) %s%s",
                                     elapsed_prefix, funcname, retval_info, base_msg);
            syslog(LOG_INFO, "%s", syslog_msg);
            free(syslog_msg);
         }
#endif
         FILE * where = (options & DBGTRC_OPTIONS_SEVERE)
                           ? thread_settings->ferr
                           : thread_settings->fout;
         f0puts(decorated_msg, where);
         f0putc('\n', where);
         fflush(where);

         free(base_msg);
         free(decorated_msg);

         msg_emitted = true;
      }
   }

   if (debug)
      printf("(%s) Done.   Returning %s\n", __func__, sbool(msg_emitted));
   return msg_emitted;
}


/** Basic function for emitting debug or trace messages.
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
      printf("(dbgtrc) Starting. trace_group=0x%04x, options=0x%02x, funcname=%s"
             " filename=%s, lineno=%d, thread=%ld, fout() %s sysout\n",
                       trace_group, options, funcname, filename, lineno, get_thread_id(),
                       (fout() == stdout) ? "==" : "!=");

   bool msg_emitted = false;
   bool in_callstack = (trace_callstack_call_depth > 0);
   if (options & DBGTRC_OPTIONS_STARTING) {
      if (!in_callstack) {
         if (is_traced_callstack_call(funcname)) {
            trace_callstack_call_depth++;
            in_callstack = true;
         }
      }
      else {
         trace_callstack_call_depth++;
         in_callstack = true;
      }
   }
   if (options & DBGTRC_OPTIONS_DONE) {
      trace_callstack_call_depth--;
   }

   if ( in_callstack || is_tracing(trace_group, filename, funcname) ) {
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
   if ( is_tracing(trace_group, filename, funcname) || trace_callstack_call_depth > 0 ) {
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
   if ( is_tracing(trace_group, filename, funcname) || trace_callstack_call_depth > 0 ) {
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


/** dbgtrc() variant that reports a return code specified as a string.
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
   if ( is_tracing(trace_group, filename, funcname) || trace_callstack_call_depth > 0 ) {
      char pre_prefix[60];
      g_snprintf(pre_prefix, 60, "Done      Returning: %s. ", retval);
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

   SYSLOG(LOG_ERR, "%s", buf2);
   SYSLOG(LOG_ERR, "%s", buffer);
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


void init_core() {
}
