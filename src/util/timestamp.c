/* timestamp.c
 *
 * Created on: Mar 12, 2017
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

/** \file
 * Timestamp Management
 */

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
 *  @return nonoseconds since start of program execution
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


/** Returns the elapsed time since start of program execution
 *  as a formatted, printable string.
 *
 *  The string is built in a thread specific private buffer.  The returned
 *  string valid until the next call of this function in the same thread.
 *
 *  @return formatted elapsed time
 */
char * formatted_elapsed_time() {
   static GPrivate  formatted_elapsed_time_key = G_PRIVATE_INIT(g_free);
   char * elapsed_buf = get_thread_fixed_buffer(&formatted_elapsed_time_key, 40);

   uint64_t et_nanos = elapsed_time_nanosec();
   uint64_t isecs    = et_nanos/ (1000 * 1000 * 1000);
   uint64_t imillis  = et_nanos/ (1000 * 1000);
   // printf("(%s) et_nanos=%"PRIu64", isecs=%"PRIu64", imillis=%"PRIu64"\n", __func__,  et_nanos, isecs, imillis);
   snprintf(elapsed_buf, 40, "%3"PRIu64".%03"PRIu64"", isecs, imillis - (isecs*1000) );

   // printf("(%s) |%s|\n", __func__, elapsed_buf);
   return elapsed_buf;
}

