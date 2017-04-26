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
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
/** \endcond */

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
   struct timespec tvNow;
   clock_gettime(CLOCK_REALTIME, &tvNow);
   // long result = (tvNow.tv_sec * 1000) + (tvNow.tv_nsec / (1000 * 1000) );  // milliseconds
   // long result = (tvNow.tv_sec * 1000 * 1000) + (tvNow.tv_nsec / 1000);     // microseconds
   uint64_t result = tvNow.tv_sec * (1000 * 1000 * 1000) + tvNow.tv_nsec;      // NANOSEC
   if (tracking_timestamps && timestamp_ct < MAX_TIMESTAMPS) {
      if (!timestamp_history) {
         timestamp_ct = 0;
         timestamp_history = calloc(MAX_TIMESTAMPS, sizeof(uint64_t));
      }
      timestamp_history[timestamp_ct++] = result;
   }
   // printf("(%s) Returning: %ld\n", result);
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
   // printf("(%s) initial_timestamp_nanos=%ld\n", __func__, initial_timestamp_nanos);
   uint64_t cur_nanos = cur_realtime_nanosec();
   if (initial_timestamp_nanos == 0)
      initial_timestamp_nanos = cur_nanos;
   uint64_t result = cur_nanos - initial_timestamp_nanos;
   // printf("(%s) Returning: %ld\n", __func__, result);
   return result;
}


/** Returns the elapsed time since start of program execution
 *  as a formatted, printable string.
 *
 *  The string is built in an internal buffer and is valid until
 *  the next call of this function.
 *
 *  @return formatted elapsed time
 */
char * formatted_elapsed_time() {
   // static char elapsed_buf1[40];
   static char elapsed_buf2[40];
   uint64_t et_nanos = elapsed_time_nanosec();
   // double secs = et_nanos/(1000.0 * 1000.0 * 1000.0);
   // snprintf(elapsed_buf1, 40, "%7.3f", secs);
   unsigned int    isecs   = et_nanos/ (1000 * 1000 * 1000);
   unsigned int    imillis = et_nanos/ (1000 * 1000);
   // printf("(%s) et_nanos=%ld, isecs=%ld, imillis=%ld\n", __func__,  et_nanos, isecs, imillis);
   // snprintf(elapsed_buf2, 40, "%3ld.%03ld", isecs, imillis - (isecs*1000) );
   snprintf(elapsed_buf2, 40, "%3d.%03d", isecs, imillis - (isecs*1000) );

   // printf("(%s) %s, %s\n", __func__, elapsed_buf1, elapsed_buf2);
   // printf("(%s) %s\n", __func__, elapsed_buf2);
   return elapsed_buf2;
}

