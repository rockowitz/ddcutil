/** @file msg_util.c
 *
 *  Creates standardized prefix (time, thread, etc.) for messages,
 *  and maintains a stack of the names of traced functions.
 */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <stdbool.h>
#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <stdio.h>
#include <syslog.h>
#include <time.h>

#include "common_inlines.h"
#include "common_printf_formats.h"
#include "glib_util.h"
#include "timestamp.h"
#include "traced_function_stack.h"

#include "msg_util.h"

bool dbgtrc_show_time      =  false;  ///< include elapsed time in debug/trace output
bool dbgtrc_show_wall_time =  false;  ///< include wall time in debug/trace output
bool dbgtrc_show_thread_id =  false;  ///< include thread id in debug/trace output
bool dbgtrc_show_process_id = false;  ///< include process id in debug/trace output
bool dbgtrc_trace_to_syslog_only = false; ///< send trace output only to system log
bool dbgtrc_trace_to_syslog = false;
bool stdout_stderr_redirected = false;

bool __thread msg_decoration_suspended = false;


/** Creates a message prefix.  Depending on settings and destination this
 *  prefix may include:
 *  - process id
 *  - thread id
 *  - wall time
 *  - elapsed time since program start
 *  - function name
 *
 *  @param buf          buffer in which to return string
 *  @param bufsz        buffer size
 *  @param dest_syslog  message destination is the system log
 */
char * get_msg_decoration(char * buf, uint bufsz, bool dest_syslog) {
   bool debug = false;
   assert(bufsz >= 100);

   if (msg_decoration_suspended) {
      buf[0] = '\0';
   }
   else {
      char elapsed_prefix[20]  = "";
      char walltime_prefix[20] = "";
      char thread_prefix[15]   = "";
      char process_prefix[15]  = "";
      char funcname_prefix[80] = "";

      if (dbgtrc_show_time)
         g_snprintf(elapsed_prefix, 20, "[%s]", formatted_elapsed_time_t(6));
      if (dbgtrc_show_wall_time && !dest_syslog)
         g_snprintf(walltime_prefix, 20, "[%s]", formatted_wall_time());
      if (dbgtrc_show_thread_id || dest_syslog)
         g_snprintf(thread_prefix, 15, PRItid, (intmax_t) tid());
      if (dbgtrc_show_process_id && !dest_syslog)
         g_snprintf(thread_prefix, 15, PRItid, (intmax_t) pid());
      if (traced_function_stack_enabled) {
         char * s = peek_traced_function();
         if (s)
            g_snprintf(funcname_prefix, 80, "(%-30s)", s);
      }

      g_snprintf(buf, bufsz, "%s%s%s%s%s",
            process_prefix, thread_prefix, walltime_prefix, elapsed_prefix, funcname_prefix);
      if (strlen(buf) > 0)
         strcat(buf, " ");
   }

   if (debug)    // can't use DBGF(), causes call to get_msg_decoration()
      printf("tid=%d, buf=%p->|%s|\n", tid(), buf, buf);

   return buf;
}


/** Returns the wall time as a formatted string.
 *
 *  The string is built in a thread specific private buffer.  The returned
 *  string is valid until the next call of this function in the same thread.
 *
 *  @return formatted wall time
 */
char * formatted_wall_time() {
   static GPrivate  formatted_wall_time_key = G_PRIVATE_INIT(g_free);
   char * time_buf = get_thread_fixed_buffer(&formatted_wall_time_key, 40);

   time_t epoch_seconds = time(NULL);
   struct tm broken_down_time;
   localtime_r(&epoch_seconds, &broken_down_time);

   strftime(time_buf, 40, "%b %d %T", &broken_down_time);

   // printf("(%s) |%s|\n", __func__, time_buf);
   return time_buf;
}

