/* util.c
 *
 * Utility functions not specific to the current application.
 *
 * This file has no dependencies on any application specific code.
 */

#include <base/parms.h>    // put first for USE_LIBEXPLAIN

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <glib.h>
#ifdef USE_LIBEXPLAIN
#include <libexplain/ioctl.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "util/file_util.h"
#include "util/string_util.h"

#include "base/linux_errno.h"

#include "base/util.h"


char * known_video_driver_modules[] = {
      "fglrx",
      "nvidia",
      "nouveau",
      "radeon",
      "vboxvideo",
      NULL
};

char * prefix_matches[] = {
      "i2c",
      "video",
      NULL
};



int query_proc_modules_for_video() {
   int rc = 0;

   GPtrArray * garray = g_ptr_array_sized_new(300);

   printf("Scanning /proc/modules for driver environment...\n");
   int ct = file_getlines("/proc/modules", garray);
   if (ct < 0)
      rc = ct;
   else {
      int ndx = 0;
      for (ndx=0; ndx<garray->len; ndx++) {
         char * curline = g_ptr_array_index(garray, ndx);
         char mod_name[32];
         int  mod_size;
         int  mod_instance_ct;
         char mod_dependencies[500];
         char mod_load_state[10];     // one of: Live Loading Unloading
         char mod_addr[30];
         int piece_ct = sscanf(curline, "%s %d %d %s %s %s",
                               mod_name,
                               &mod_size,
                               &mod_instance_ct,
                               mod_dependencies,
                               mod_load_state,
                               mod_addr);
         if (piece_ct != 6) {
            printf("(%s) Unexpected error parsing /proc/modules.  sscanf returned %d\n", __func__, piece_ct);
         }
         if (streq(mod_name, "drm") ) {
            printf("   Loaded drm module depends on: %s\n", mod_dependencies);
         }
         else if (exactly_matches_any(mod_name, known_video_driver_modules) >= 0 ) {
            printf("   Found video driver module: %s\n", mod_name);
         }
         else if ( starts_with_any(mod_name, prefix_matches) >= 0 ) {
            printf("   Found other loaded module: %s\n", mod_name);
         }
      }
   }

   FILE * fp = fopen("/proc/version", "r");
   if (!fp) {
      fprintf(stderr, "Error opening /proc/version: %s", strerror(errno));
   }
   else {
      char * version_line = NULL;
      size_t len = 0;
      ssize_t read;
      // just one line:
      read = getline(&version_line, &len, fp);
      if (read == -1) {
         printf("Nothing to read from /proc/version\n");
      }
      else {
         printf("\n%s\n", version_line);
      }
   }

   return rc;
}



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
      printf("(%s) total timestamps: %d\n", __func__, timestamp_ct);
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
      printf("(%s) Not tracking timestamps\n", __func__);
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

