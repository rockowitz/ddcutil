/* util.c
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


#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <glib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <linux/limits.h>
#include <limits.h>
#include <sys/stat.h>

#include "util/file_util.h"
#include "util/string_util.h"

#include "base/linux_errno.h"
#include "base/msg_control.h"
#include <base/parms.h>

#include "base/util.h"


//
// Timing functions
//

// For debugging timestamp generation, maintain a timestamp history.
#define MAX_TIMESTAMPS 1000
long  timestamp[MAX_TIMESTAMPS];
int   timestamp_ct = 0;
bool  tracking_timestamps = false;    // set true to enable timestamp tracking


// Returns the current value of the realtime clock in nanoseconds.
long cur_realtime_nanosec() {
   struct timespec tvNow;
   clock_gettime(CLOCK_REALTIME, &tvNow);
   // long result = (tvNow.tv_sec * 1000) + (tvNow.tv_nsec / (1000 * 1000) );  // milliseconds
   // long result = (tvNow.tv_sec * 1000 * 1000) + (tvNow.tv_nsec / 1000);     // microseconds
   long result = tvNow.tv_sec * (1000 * 1000 * 1000) + tvNow.tv_nsec;          // NANOSEC
   if (tracking_timestamps && timestamp_ct < MAX_TIMESTAMPS)
      timestamp[timestamp_ct++] = result;
   // printf("(%s) Returning: %ld\n", result);
   return result;
}


void report_timestamp_history() {
   if (tracking_timestamps) {
      DBGMSG("total timestamps: %d", timestamp_ct);
      bool monotonic = true;
      int ctr = 0;
      for (; ctr < timestamp_ct; ctr++) {
         printf("  timestamp[%d] =  %15ld\n", ctr, timestamp[ctr] );
         if (ctr > 0 && timestamp[ctr] <= timestamp[ctr-1]) {
            printf("   !!! NOT STRICTLY MONOTONIC !!!\n");
            monotonic = false;
         }
      }
      printf("Timestamps are%s strictly monotonic\n", (monotonic) ? "" : " NOT");
   }
   else
      DBGMSG("Not tracking timestamps");
}


//
// Standardized mechanisms for handling exceptional conditions, including
// error messages and possible program termination.
//

void report_ioctl_error(
      int   errnum,
      const char* funcname,   // const to avoid warning msg on references at compile time
      int   lineno,
      char* filename,
      bool fatal) {
   int errsv = errno;
   // fprintf(stderr, "(report_ioctl_error)\n");
   fprintf(stderr, "ioctl error in function %s at line %d in file %s: errno=%s\n",
           funcname, lineno, filename, linux_errno_desc(errnum) );
   // fprintf(stderr, "  %s\n", strerror(errnum));  // linux_errno_desc now calls strerror
   // will contain at least sterror(errnum), possibly more:
   // not worth the linkage issues:
   // fprintf(stderr, "  %s\n", explain_errno_ioctl(errnum, filedes, request, data));
   if (fatal)
      exit(EXIT_FAILURE);
   errno = errsv;
}



void report_ioctl_error2(
      int   errnum,
      int   fh,
      int   request,
      void* data,
      const char* funcname,   // const to avoid warning msg on references at compile time
      int   lineno,
      char* filename,
      bool fatal)
{
   int errsv = errno;
   // fprintf(stderr, "(report_ioctl_error2)\n");
   report_ioctl_error(errno, funcname, lineno, filename, false /* non-fatal */ );
#ifdef USE_LIBEXPLAIN
   // fprintf(stderr, "(report_ioctl_error2) within USE_LIBEXPLAIN\n");
   fprintf(stderr, "%s\n", explain_ioctl(fh, request, data));
#endif
   if (fatal)
      exit(EXIT_FAILURE);
   errno = errsv;
}



/* Called when a condition that should be impossible has been detected.
 * Issues messages to stderr and terminates execution.
 *
 * This function is normally invoked using macro PROGRAM_LOGIC_ERROR
 * defined in util.h.
 *
 * Arguments:
 *    funcname    function name
 *    lineno      line number in source file
 *    fn          source file name
 *    format      format string, as in printf()
 *    ...         or or more substitution values for the format string
 *
 * Returns:
 *    nothing (terminates execution)
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

  // assemble the location message:
  char buf2[250];
  snprintf(buf2, 250, "Program logic error in function %s at line %d in file %s:\n",
                      funcname, lineno, fn);

  // don't combine into 1 line, might be very long.  just output 2 lines:
  fputs(buf2,   stderr);
  fputs(buffer, stderr);
  fputc('\n',   stderr);

  fputs("Terminating execution.\n", stderr);
  exit(EXIT_FAILURE);
}

