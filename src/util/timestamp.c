/** @file timestamp.c
 *
 *  Timestamp management
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#define _GNU_SOURCE

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
/** \endcond */

#include "glib_util.h"
#include "string_util.h"

#include "timestamp.h"

//
// Timestamp Generation
//

/** For debugging timestamp generation, maintain a timestamp history. */
bool  tracking_timestamps = false;    // set true to enable timestamp history
#define MAX_TIMESTAMPS 1000
// static long  timestamp[MAX_TIMESTAMPS];
static int   timestamp_ct = 0;
static uint64_t * timestamp_history = NULL;


/** Returns the current value of the realtime clock in nanoseconds.
 *
 * @return timestamp, in nanoseconds
 *
 * @remark
 * If debugging timestamp generation, the timestamp is remembered.
 */
uint64_t cur_realtime_nanosec() {
   // on Pi, __time_t resolves to long int
   struct timespec tvNow;
   clock_gettime(CLOCK_REALTIME, &tvNow);
   // long result = (tvNow.tv_sec * 1000) + (tvNow.tv_nsec / (1000 * 1000) );  // milliseconds
   // long result = (tvNow.tv_sec * 1000 * 1000) + (tvNow.tv_nsec / 1000);     // microseconds

   // wrong on 32
   // uint64_t result0 = tvNow.tv_sec * (1000 * 1000 * 1000) + tvNow.tv_nsec;      // NANOSEC
   // printf("(%s) result0=%"PRIu64"\n", __func__, result0);

   // wrong on 32
   // uint64_t result1 = tvNow.tv_sec * (1000 * 1000 * 1000);
   // printf("(%s) result1=%"PRIu64"\n", __func__, result1);

   // ok
   uint64_t result = tvNow.tv_sec * (uint64_t)(1000*1000*1000);
   // printf("(%s) result=%"PRIu64"\n", __func__, result);

   // ok
   // uint64_t result3 = tvNow.tv_sec * (uint64_t)1000000000;
   // printf("(%s) result3=%"PRIu64"\n", __func__, result3);

   // wrong value on 32 bit
   // uint64_t result4 = tvNow.tv_sec * (uint64_t)1000000000 + tvNow.tv_sec;
   // printf("(%s) result4=%"PRIu64"\n", __func__, result4);

   // wrong value on 32 bit
   // uint64_t result6 = (tvNow.tv_sec * (uint64_t)1000000000) + (uint64_t)tvNow.tv_sec;
   // printf("(%s) result6=%"PRIu64"\n", __func__, result6);

   // uint64_t result2 = tvNow.tv_sec;
   // printf("(%s) result2=%"PRIu64"\n", __func__, result2);

   // result2 *= (1000*1000*1000);
   // printf("(%s) result2=%"PRIu64"\n", __func__, result2);

   // must do addition separately on 32 bit, ow. get bad value
   result += tvNow.tv_nsec;
   // printf("(%s) result=%"PRIu64"\n", __func__, result);

   if (tracking_timestamps && timestamp_ct < MAX_TIMESTAMPS) {
      if (!timestamp_history) {
         timestamp_ct = 0;
         timestamp_history = calloc(MAX_TIMESTAMPS, sizeof(uint64_t));
      }
      timestamp_history[timestamp_ct++] = result;
   }
   // printf("(%s) tv_sec=%ld, tv_nsec=%10ld, Returning: %"PRIu64"\n",
   //        __func__, tvNow.tv_sec, tvNow.tv_nsec, result);
   return result;
}


/** Reports history of generated timestamps
 *
 * @remark
 * This is a debugging function.
 */
void show_timestamp_history() {
   if (tracking_timestamps && timestamp_history) {
      // n. DBGMSG writes to FOUT
      printf("Total timestamps: %d\n", timestamp_ct);
      bool monotonic = true;
      int ctr = 0;
      for (; ctr < timestamp_ct; ctr++) {
         printf("  timestamp[%d] =  %15" PRIu64 "\n", ctr, timestamp_history[ctr] );
         if (ctr > 0 && timestamp_history[ctr] <= timestamp_history[ctr-1]) {
            printf("   !!! NOT STRICTLY MONOTONIC !!!\n");
            monotonic = false;
         }
      }
      printf("Timestamps are%s strictly monotonic\n", (monotonic) ? "" : " NOT");
   }
   else
      printf("Not tracking timestamps\n");
}


static uint64_t initial_timestamp_nanos = 0;


/** Returns the elapsed time in nanoseconds since the start of
 *  program execution.
 *
 *  The first call to this function marks the start of program
 *  execution and returns 0.
 *
 *  @return nanoseconds since start of program execution
 */
uint64_t elapsed_time_nanosec() {
   // printf("(%s) initial_timestamp_nanos=%"PRIu64"\n", __func__, initial_timestamp_nanos);
   uint64_t cur_nanos = cur_realtime_nanosec();
   if (initial_timestamp_nanos == 0)
      initial_timestamp_nanos = cur_nanos;
   uint64_t result = cur_nanos - initial_timestamp_nanos;
   // printf("(%s) Returning: %"PRIu64"\n", __func__, result);
   return result;
}


static uint64_t ipow(const uint64_t base, guint n)
{
   int p = base;
   if (n == 0)
      return 1;
   for (int i = 1; i < n; ++i)
      p *= base;
   return p;
}


/** Returns the elapsed time in seconds since start of program execution
 *  as a formatted, printable string.
 *
 *  The string is built in a thread specific private buffer.  The returned
 *  string is valid until the next call of this function in the same thread.
 *
 *  @param  precision  number of digits after the decimal point
 *  @return formatted elapsed time
 */
char * formatted_elapsed_time_t(guint precision) {
   static GPrivate  formatted_elapsed_time_key = G_PRIVATE_INIT(g_free);
   char * elapsed_buf = get_thread_fixed_buffer(&formatted_elapsed_time_key, 40);

   uint64_t et_nanos         = elapsed_time_nanosec();
   uint64_t isecs            = et_nanos/ (1000 * 1000 * 1000);
   uint64_t adjusted_isecs   = isecs * ipow(10,precision);
   uint64_t fractional_units = et_nanos / ipow(10, (9-precision));

   // printf("(%s) et_nanos=%"PRIu64", isecs=%"PRIu64", fractional_units=%"PRIu64", adjusted_isecs=%"PRIu64"\n",
   //       __func__,  et_nanos,       isecs,                      fractional_units, adjusted_isecs);
   snprintf(elapsed_buf, 40, "%3"PRIu64".%0*"PRIu64"", isecs, precision, (fractional_units-adjusted_isecs) );

   // printf("(%s) |%s|\n", __func__, elapsed_buf);
   return elapsed_buf;
}


/** Returns returns a time in nanoseconds as a formatted, printable string
 *  in the form SECONDS.MILLISECONDS.
 *
 *  The string is built in a thread specific private buffer.  The returned
 *  string is valid until the next call of this function in the same thread.
 *
 *  @param  start   start time, in nanoseconds
 *  @param  end     end time, in nanoseconds
 *  @return formatted time difference
 */
char *   formatted_time_t(uint64_t nanos) {
   static GPrivate  formatted_time_key = G_PRIVATE_INIT(g_free);
   char * elapsed_buf = get_thread_fixed_buffer(&formatted_time_key, 40);

   uint64_t isecs    = nanos/ (1000 * 1000 * 1000);
   uint64_t imillis  = nanos/ (1000 * 1000);
   snprintf(elapsed_buf, 40, "%3"PRIu64".%03"PRIu64"", isecs, imillis - (isecs*1000) );
   return elapsed_buf;
}


/** Thread safe function that returns a string representation of an epoch
 *  time value. The returned value is valid until the next call to this
 *  function on the current thread.
 *
 *  \param  epoch_time
 *  \return string representation
 */
char * formatted_epoch_time_t(long epoch_seconds) {
   static GPrivate  formatted_epoch_time_key = G_PRIVATE_INIT(g_free);
   char * buf = get_thread_fixed_buffer(&formatted_epoch_time_key, 40);

   struct tm broken_down_time;
   localtime_r(&epoch_seconds, &broken_down_time);
   strftime(buf, 40, "%b %d %T", &broken_down_time);

   return buf;
}


