/** @file msg_util.c */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <stdbool.h>
#include <glib-2.0/glib.h>
#include <inttypes.h>
#include <sys/syscall.h>
#include <time.h>

#include "debug_util.h"
#include "glib_util.h"
#include "timestamp.h"

#include "msg_util.h"

bool dbgtrc_show_time      =  false;  ///< include elapsed time in debug/trace output
bool dbgtrc_show_wall_time =  false;  ///< include wall time in debug/trace output
bool dbgtrc_show_thread_id =  false;  ///< include thread id in debug/trace output
bool dbgtrc_show_process_id = false;  ///< include process id in debug/trace output
bool dbgtrc_trace_to_syslog_only = false; ///< send trace output only to system log
bool stdout_stderr_redirected = false;

bool dbgtrc_dest_syslog     = false;

__thread pid_t process_id = 0;
__thread pid_t thread_id  = 0;

static inline pid_t tid() {
   if (!thread_id)
      thread_id = syscall(SYS_gettid);
   return thread_id;
}

static inline pid_t pid() {
   if (!process_id)
      process_id = syscall(SYS_gettid);
   return thread_id;
}


char * get_msg_decoration(char * buf, uint bufsize, bool dest_syslog) {
   bool debug = false;
   assert(bufsize >= 70);

   char elapsed_prefix[20]  = "";
   char walltime_prefix[20] = "";
   char thread_prefix[15]   = "";
   char process_prefix[15]  = "";

   if (dbgtrc_show_time)
      g_snprintf(elapsed_prefix, 20, "[%s]", formatted_elapsed_time_t(4));
   if (dbgtrc_show_wall_time && !dbgtrc_dest_syslog)
      g_snprintf(walltime_prefix, 20, "[%s]", formatted_wall_time());
   if (dbgtrc_show_thread_id)
      g_snprintf(thread_prefix, 15, "[%7jd]", (intmax_t) tid());
   if (dbgtrc_show_process_id)
      g_snprintf(thread_prefix, 15, "[%7jd]", (intmax_t) pid());

   g_snprintf(buf, bufsize, "%s%s%s%s", process_prefix, thread_prefix, walltime_prefix, elapsed_prefix);
   if (strlen(buf) > 0)
      strcat(buf, " ");

   if (debug)    // can't use DBGF(), causes call to get_msg_decoration()
      printf("buf=%p->|%s|\n", buf, buf);

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

