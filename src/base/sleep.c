/** \file sleep.c
 * Sleep Management
 *
 * Sleeps are integral to the DDC protocol.  Most of **ddcutil's** elapsed
 * time is spent in sleeps mandated by the DDC protocol.
 * Sleep invocation is centralized here to keep statistics and facilitate
 * future tuning.
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
// Sleep and sleep statistics
//

static Sleep_Stats sleep_stats;
static bool trace_sleep = true;    // consider controlling this by function enable_trace_sleep()

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

/** Sleep for the specified number of milliseconds.
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


void sleep_millis_with_tracex(
        int          milliseconds,
        const char * func,
        int          lineno,
        const char * filename,
        const char * message)
{
   bool debug = false;


   if (trace_sleep) {
      char smsg[200];

      if (message)
         g_snprintf(smsg, 200, ", %s", message);
      else
         smsg[0] = '\0';
      // printf("%sSleeping for %d milliseconds%s\n", sloc, milliseconds, smsg);
      dbgtrc((debug) ? 0xff : DDCA_TRC_SLEEP, func, lineno, filename, "Sleeping for %d milliseconds%s", milliseconds, smsg);
   }

   sleep_millis(milliseconds);
}


/** Sleep for the specified number of milliseconds, with tracing
 *
 * \param milliseconds number of milliseconds to sleep
 * \param caller_location name of calling function
 * \param message trace message
 *
 * Tracing is only performed if static variable trace_sleep
 * is set to **true**.
 */
void sleep_millis_with_trace(
        int          milliseconds,
        const char * caller_location,
        const char * message)
{
   // bool debug = true;


   if (trace_sleep) {
      char sloc[100];
      char smsg[200];

      if (caller_location)
         snprintf(sloc, 100, "(%-25s) ", caller_location);
      else
         sloc[0] = '\0';

      if (message)
         g_snprintf(smsg, 200, ", %s", message);
      else
         smsg[0] = '\0';
      // printf("%sSleeping for %d milliseconds%s\n", sloc, milliseconds, smsg);
      dbgtrc(DDCA_TRC_SLEEP, caller_location, 0, NULL, "Sleeping for %d milliseconds%s", milliseconds, smsg);
   }

   sleep_millis(milliseconds);
}

