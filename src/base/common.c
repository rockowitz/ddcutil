/* common.c
 *
 * Created on: Jun 21, 2014
 *     Author: rock
 *
 * This file contains basic code used throughout the ddctool application.
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


#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "util/report_util.h"

#include "base/msg_control.h"

#include "base/common.h"


//
// Output format control
//

#ifdef OLD
static Output_Format output_format = OUTPUT_NORMAL;

char * output_format_name(Output_Format format)  {
   char * result = "UNSET";
   switch (format) {
   case OUTPUT_NORMAL:        result = "OUTPUT_NORMAL";       break;
   case OUTPUT_PROG_VCP:      result = "OUTPUT_PROG_VCP";      break;
   case OUTPUT_PROG_BUSINFO:  result = "OUTPUT_PROG_BUSINFO";  break;
   }
   return result;
}

void set_output_format(Output_Format format) {
   output_format = format;
}

Output_Format get_output_format() {
   return output_format;
}
#endif

//
// Sleep and sleep statistics
//

static Sleep_Stats sleep_stats;


void init_sleep_stats() {
   sleep_stats.total_sleep_calls = 0;
   sleep_stats.requested_sleep_milliseconds = 0;
   sleep_stats.actual_sleep_nanos = 0;
}


Sleep_Stats * get_sleep_stats() {
   return &sleep_stats;
}


void report_sleep_stats(int depth) {
   int d1 = depth+1;
   rpt_title("Sleep Call Stats:", depth);
   rpt_printf(d1, "Total sleep calls:                           %10d",  sleep_stats.total_sleep_calls);
   rpt_printf(d1, "Requested sleep time milliseconds :          %10ld", sleep_stats.requested_sleep_milliseconds);
   rpt_printf(d1, "Actual sleep milliseconds (nanosec):         %10ld  (%10ld)",
          sleep_stats.actual_sleep_nanos / (1000*1000),
          sleep_stats.actual_sleep_nanos);
}


// SleepMilliseconds specifies time in milliseconds, usleep takes microseconds
void sleep_millis( int milliseconds) {
   long start_nanos = cur_realtime_nanosec();
   usleep(milliseconds*1000);
   sleep_stats.actual_sleep_nanos += (cur_realtime_nanosec()-start_nanos);
   sleep_stats.requested_sleep_milliseconds += milliseconds;
   sleep_stats.total_sleep_calls++;
}


void sleep_millis_with_trace(int milliseconds, const char * caller_location, const char * message) {
   bool trace_sleep = false;

   if (trace_sleep) {
      char sloc[100];
      char smsg[200];

      if (caller_location)
         snprintf(sloc, 100, "(%s) ", caller_location);
      else
         sloc[0] = '\0';
      if (message)
         snprintf(smsg, 200, "%s. ", message);
      else
         smsg[0] = '\0';
      printf("%s%sSleeping for %d milliseconds\n", sloc, smsg, milliseconds);
   }

   sleep_millis(milliseconds);
}


//
// Error handling
//

void terminate_execution_on_error(
        Trace_Group   trace_group,
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...)
{
   char buffer[200];
   char buf2[250];
   char * finalBuffer = buffer;
   va_list(args);
   va_start(args, format);
   vsnprintf(buffer, 200, format, args);

   if ( is_tracing(trace_group, fn) ) {
      snprintf(buf2, 250, "(%s) %s", funcname, buffer);
      finalBuffer = buf2;
   }

   puts(finalBuffer);
   puts("Terminating execution.");
   exit(EXIT_FAILURE);
}


