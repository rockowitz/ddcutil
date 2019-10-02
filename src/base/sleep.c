/** \file sleep.c
 * Basic Sleep Services
 *
 * Most of **ddcutil's** elapsed time is spent in sleeps mandated by the
 * DDC protocol. Basic sleep invocation is centralized here to perform sleep
 * tracing and and maintain sleep statistics.
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


/** \cond */
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
/** \endcond */

#include "util/report_util.h"
#include "util/timestamp.h"

#include "base/core.h"
#include "base/sleep.h"


//
// Sleep statistics
//

static Sleep_Stats sleep_stats;

/** Sets all sleep statistics to 0. */
void init_sleep_stats() {
   sleep_stats.total_sleep_calls = 0;
   sleep_stats.requested_sleep_milliseconds = 0;
   sleep_stats.actual_sleep_nanos = 0;
}


/** Returns the current sleep statistics
 *
 * \return the current value of the accumulated sleep stats
 */
Sleep_Stats get_sleep_stats() {
   return sleep_stats;
}


/** Reports the accumulated sleep statistics
 *
 * \param depth logical indentation depth
 */
void report_sleep_stats(int depth) {
   int d1 = depth+1;
   rpt_title("Sleep Call Stats:", depth);
   rpt_vstring(d1, "Total sleep calls:                              %10d",
                   sleep_stats.total_sleep_calls);
   rpt_vstring(d1, "Requested sleep time milliseconds :             %10d",
                   sleep_stats.requested_sleep_milliseconds);
   rpt_vstring(d1, "Actual sleep milliseconds (nanosec):            %10"PRIu64"  (%13" PRIu64 ")",
                   sleep_stats.actual_sleep_nanos / (1000*1000),
                   sleep_stats.actual_sleep_nanos);
}


//
// Perform Sleep
//

/** Sleep for the specified number of milliseconds and
 *  record sleep statistics.
 *
 * \param milliseconds number of milliseconds to sleep
 */
void sleep_millis(int milliseconds) {
   uint64_t start_nanos = cur_realtime_nanosec();
   usleep(milliseconds*1000);   // usleep takes microseconds, not milliseconds
   sleep_stats.actual_sleep_nanos += (cur_realtime_nanosec()-start_nanos);
   sleep_stats.requested_sleep_milliseconds += milliseconds;
   sleep_stats.total_sleep_calls++;
}


/** Sleep for the specified number of milliseconds, record
 *  sleep statistics, and perform tracing.
 *
 *  Tracing occurs if trace group DDCA_TRC_SLEEP is enabled.
 *
 * \param milliseconds number of milliseconds to sleep
 * \param func name of function that invoked sleep
 * \param lineno line number in file where sleep was invoked
 * \param filename name of file from which sleep was invoked
 * \param text to be appended to trace message
 */
void sleep_millis_with_tracex(
        int          milliseconds,
        const char * func,
        int          lineno,
        const char * filename,
        const char * message)
{
   bool debug = false;

   if (!message)
      message = "";

   dbgtrc((debug) ? 0xff : DDCA_TRC_SLEEP,
           func, lineno, filename, "Sleeping for %d milliseconds%s", milliseconds, message);

   sleep_millis(milliseconds);
}
