/** @file sleep.c Basic Sleep Services
 *
 * Most of **ddcutil's** elapsed time is spent in sleeps mandated by the
 * DDC protocol. Basic sleep invocation is centralized here to perform sleep
 * tracing and and maintain sleep statistics.
 */

// Copyright (C) 2014-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
/** \endcond */

#include "util/common_inlines.h"
#include "util/debug_util.h"
#include "util/report_util.h"
#include "util/timestamp.h"

#include "public/ddcutil_types.h"

#include "base/core.h"
#include "base/sleep.h"


//
// Sleep statistics
//

static Sleep_Stats sleep_stats;
G_LOCK_DEFINE(sleep_stats);

/** Sets all sleep statistics to 0. */
void init_sleep_stats() {
   G_LOCK(sleep_stats);
   sleep_stats.total_sleep_calls = 0;
   sleep_stats.requested_sleep_milliseconds = 0;
   sleep_stats.actual_sleep_nanos = 0;
   G_UNLOCK(sleep_stats);
}


/** Returns the current sleep statistics
 *
 * \return a copy of struct Sleep_Stats, containing thee current value
 *         of the accumulated sleep stats
 *
 * \remark
 * N.B.  Returns a copy of the struct on the stack, not a pointer to the struct.
 */
Sleep_Stats get_sleep_stats() {
   Sleep_Stats stats_copy;
   G_LOCK(sleep_stats);
   stats_copy = sleep_stats;
   G_UNLOCK(sleep_stats);
   return stats_copy;;
}


/** Reports the accumulated sleep statistics
 *
 * \param depth logical indentation depth
 */
void report_sleep_stats(int depth) {
   Sleep_Stats stats_copy = get_sleep_stats();
   int d1 = depth+1;
   rpt_title("Sleep Call Stats:", depth);
   rpt_vstring(d1, "Total sleep calls:                              %10d",
                   stats_copy.total_sleep_calls);
   rpt_vstring(d1, "Requested sleep time milliseconds :             %10d",
                   stats_copy.requested_sleep_milliseconds);
   rpt_vstring(d1, "Actual sleep milliseconds (nanosec):            %10"PRIu64"  (%13" PRIu64 ")",
                   stats_copy.actual_sleep_nanos / (1000*1000),
                   stats_copy.actual_sleep_nanos);
}


//
// Perform Sleep
//

/** General function for performing sleep.
 *
 *  This is a merger of the several sleep function variants previously defined
 *  in sleep.c
 *
 * Sleep function variants are implemented as macros.
 */
void loggable_sleep(
      int                    millisec,
      Loggable_Sleep_Options opts,
      DDCA_Syslog_Level      syslog_level,
      const char *           func,
      int                    lineno,
      const char *           filename,
      const char *           format, ...)
{
   bool debug = false;
   char * message = NULL;

   if (opts & SLEEP_OPT_TRACEABLE || syslog_level > DDCA_SYSLOG_NEVER) {
      va_list(args);
      va_start(args, format);
      message = g_strdup_vprintf(format, args);
      va_end(args);
   }

   bool only_syslog = syslog_level > DDCA_SYSLOG_NEVER && dbgtrc_trace_to_syslog_only;
   if (opts & SLEEP_OPT_TRACEABLE && !only_syslog) {
      DBGTRC_EXECUTED(debug, DDCA_TRC_SLEEP,
                      "Sleeping for %d milliseconds. %s", millisec, message);
   }

   if (syslog > DDCA_SYSLOG_NEVER) {
      // Alternatively, use syslog() instead of SYSLOG2() to ensure that msg is
      // written to system log no matter what ddcutil log level cutoff is in effect
#ifdef W_TID
      SYSLOG2(syslog_level, "[%d](%s) Sleeping for %d milliseconds: %s", tid(), func, millisec, message);
#else
      SYSLOG2(syslog_level, "(%s) %s: Sleeping for %d milliseconds", func, message, millisec);
#endif
   }

   if (message)
      free(message);

   uint64_t start_nanos  = cur_realtime_nanosec();
   uint64_t nanosec = millisec * 1000;
   if (nanosec > 0) {
      usleep(nanosec);   // usleep takes microseconds, not milliseconds
      if (opts&SLEEP_OPT_STATS) {
         G_LOCK(sleep_stats);
         sleep_stats.actual_sleep_nanos += (cur_realtime_nanosec()-start_nanos);
         sleep_stats.requested_sleep_milliseconds += millisec;
         sleep_stats.total_sleep_calls++;
         G_UNLOCK(sleep_stats);
      }
   }
}


#ifdef OLD
/** Sleep for the specified number of milliseconds and
 *  record sleep statistics.
 *
 * \param milliseconds number of milliseconds to sleep
 */
void sleep_millis(int milliseconds) {
   uint64_t start_nanos = cur_realtime_nanosec();
   usleep(milliseconds*1000);   // usleep takes microseconds, not milliseconds
   G_LOCK(sleep_stats);
   sleep_stats.actual_sleep_nanos += (cur_realtime_nanosec()-start_nanos);
   sleep_stats.requested_sleep_milliseconds += milliseconds;
   sleep_stats.total_sleep_calls++;
   G_UNLOCK(sleep_stats);
}
#endif

#ifdef OLD
/** Sleep for the specified number of milliseconds, record
 *  sleep statistics, and perform tracing.
 *
 *  Tracing occurs if trace group DDCA_TRC_SLEEP is enabled.
 *
 * \param milliseconds number of milliseconds to sleep
 * \param func         name of function that invoked sleep
 * \param lineno       line number in file where sleep was invoked
 * \param filename     name of file from which sleep was invoked
 * \param message      text to be appended to trace message
 */
void sleep_millis_with_trace(
        int          milliseconds,
        const char * func,
        int          lineno,
        const char * filename,
        const char * message)
{
   bool debug = false;

   if (!message)
      message = "";

   DBGTRC_EXECUTED(debug, DDCA_TRC_SLEEP,
                   "Sleeping for %d milliseconds. %s", milliseconds, message);

   if (milliseconds > 0)
      sleep_millis(milliseconds);
}
#endif


#ifdef OLD
// Special sleep function for watching display connection changes
// TODO: Integrate with sleep_millis_with_trace()

void sleep_millis_with_syslog(DDCA_Syslog_Level level,
                     const char *      func,
                     int               line,
                     const char *      file,
                     uint              millis,
                     const char *      msg)
{
   bool debug = false;
   DBGMSF(debug, "func=%s, millis=%d, micros=%ld", func, millis, MILLIS2MICROS(millis));
   usleep((uint64_t)1000*millis);
   // Alternatively, use syslog() instead of SYSLOG2() to ensure that msg is
   // written to system log no matter what ddcutil log level cutoff is in effect
#ifdef W_TID
   SYSLOG2(level, "[%d](%s) Slept for %d millisec: %s", tid(), func, millis, msg);
#endif
   SYSLOG2(level, "(%s) %s: Slept for %d millisec", func, msg, millis);
}

#endif
