/** @file debug_util.c
 *
 * Functions for debugging
 */

// Copyright (C) 2016-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"
/** \cond */
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#ifdef UNUSED
#ifdef TARGET_BSD
#include <pthread_np.h>
#else
#include <sys/types.h>
#include <sys/syscall.h>
#include <syslog.h>
#endif
#endif

/** \endcond */

#include "backtrace.h"
#include "report_util.h"
#include "string_util.h"

#include "debug_util.h"


void show_backtrace(int stack_adjust) {
   int depth = 0;
   GPtrArray * callstack = get_backtrace(stack_adjust+2); // +2 for get_backtrace(), backtrace()
   if (!callstack) {
      perror("backtrace() unavailable");
   }
   else {
      rpt_label(depth, "Current call stack (using backtrace()):");
      for (int ndx = 0; ndx < callstack->len; ndx++) {
         rpt_vstring(depth, "   %s", (char *) g_ptr_array_index(callstack, ndx));
      }
      g_ptr_array_set_free_func(callstack, g_free);
      g_ptr_array_free(callstack, true);
   }
}


static int min_funcname_size = 32;

void set_simple_dbgmsg_min_funcname_size(int new_size) {
   min_funcname_size = new_size;
}

// n. uses no report_util functions
bool simple_dbgmsg(
        bool              debug_flag,
        const char *      funcname,
        const int         lineno,
        const char *      filename,
        const char *      format,
        ...)
{
   bool debug_func = false;
   if (debug_func)
      printf("(simple_dbgmsg) Starting. debug_flag=%s, funcname=%s filename=%s, lineno=%d\n",
                       sbool(debug_flag), funcname, filename, lineno);

#ifdef UNUSED
   char thread_prefix[15] = "";
    int tid = pthread_getthreadid_np();
       pid_t pid = syscall(SYS_getpid);
       snprintf(thread_prefix, 15, "[%7jd]", (intmax_t) pid);  // is this proper format for pid_t
#endif

   bool msg_emitted = false;
   if ( debug_flag ) {
      va_list(args);
      va_start(args, format);
      char * buffer = g_strdup_vprintf(format, args);
      va_end(args);

      char * buf2 = g_strdup_printf("(%-*s) %s", min_funcname_size, funcname, buffer);

      // f0puts(buf2, stdout);
      // f0putc('\n', stdout);
      rpt_vstring(0, "%s", buf2);
      fflush(stdout);
      free(buffer);
      free(buf2);
      msg_emitted = true;
   }

   return msg_emitted;
}
