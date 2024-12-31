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
bool __thread msg_decoration_suspended = false;
bool traced_function_stack_enabled = true;
bool __thread traced_function_stack_suspended = false;

static __thread GQueue * traced_function_stack;

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
   assert(bufsize >= 100);

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
         g_snprintf(elapsed_prefix, 20, "[%s]", formatted_elapsed_time_t(4));
      if (dbgtrc_show_wall_time && !dbgtrc_dest_syslog)
         g_snprintf(walltime_prefix, 20, "[%s]", formatted_wall_time());
      if (dbgtrc_show_thread_id)
         g_snprintf(thread_prefix, 15, "[%7jd]", (intmax_t) tid());
      if (dbgtrc_show_process_id)
         g_snprintf(thread_prefix, 15, "[%7jd]", (intmax_t) pid());
      if (traced_function_stack_enabled) {
         char * s = peek_traced_function();
         if (s)
            g_snprintf(funcname_prefix, 80, "(%-40s)", s);
      }

      g_snprintf(buf, bufsize, "%s%s%s%s%s",
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


void debug_traced_function_stack(bool reverse) {
   if (traced_function_stack) {
      printf("[%d] Traced function stack %p:\n", tid(), traced_function_stack);
      int queue_len = g_queue_get_length(traced_function_stack);
      if (queue_len > 0) {
         printf("traced function stack (addr=%p, len=%d, tid=%d):\n", traced_function_stack, queue_len, tid() );
         if (reverse) {
            for (int ndx =  g_queue_get_length(traced_function_stack)-1; ndx >=0 ; ndx--) {
               printf("   %s\n", (char*) g_queue_peek_nth(traced_function_stack, ndx));
            }
         }
         else {
            for (int ndx = 0; ndx < g_queue_get_length(traced_function_stack); ndx++) {
               printf("   %s\n", (char*) g_queue_peek_nth(traced_function_stack, ndx));
            }
         }
      }
      else {
         printf("    EMPTY\n");
      }
   }
   else {
      printf("[%d] no traced function stack\n", tid());
   }
}


GPtrArray * get_traced_callstack(bool most_recent_last) {
   GPtrArray* callstack = g_ptr_array_new_with_free_func(g_free);
   int qsize = g_queue_get_length(traced_function_stack);
   if (most_recent_last) {
      for (int ndx = 0; ndx < qsize; ndx++) {
         g_ptr_array_add(callstack, strdup((char*) g_queue_peek_nth(traced_function_stack, ndx)));
      }
   }
   else {
      for (int ndx = qsize-1; ndx >= 0; ndx--) {
          g_ptr_array_add(callstack, strdup((char*) g_queue_peek_nth(traced_function_stack, ndx)));
       }
   }
   return callstack;
}


void push_traced_function(const char * funcname) {
   bool debug = false;
   if (debug)
      printf("[%d](push_traced_function) funcname = %s, traced_function_stack_enabled=%d\n",
            tid(), funcname, traced_function_stack_enabled);
   if (traced_function_stack_enabled && !traced_function_stack_suspended) {
      if (!traced_function_stack) {
         traced_function_stack = g_queue_new();
         if (debug)
            printf("(push_traced_function) tid=%d, allocated new traced_function_stack %p\n", tid(), traced_function_stack);
      }
      g_queue_push_head(traced_function_stack, g_strdup(funcname));
   }
   else {
      // fprintf(stderr, "traced_function_stack is disabled\n");
   }
   // debug_traced_function_stack(true);
}


char * peek_traced_function() {
   char * result = NULL;
   if (traced_function_stack) {
      result = g_queue_peek_head(traced_function_stack);   // or g_strdup() ???
   }
   // printf("(peek_traced_function) tid=%d, returning %s\n", tid(), result);
   return result;
}


void pop_traced_function(const char * funcname) {
   if (!traced_function_stack) {
      fprintf(stderr, "(%s) tid=%d. No traced function stack\n", __func__, tid());
   }
   else {
      if (traced_function_stack_enabled && !traced_function_stack_suspended) {
         char * popped_func = g_queue_pop_head(traced_function_stack);
         if (!popped_func) {
            fprintf(stderr, "(%s) tid=%d, traced_function_stack=%p, traced_function_stack is empty\n",
                  __func__, tid(), traced_function_stack);
         }
         else if (strcmp(popped_func, funcname) != 0) {
            fprintf(stderr, "[%d](%s)traced_function_stack=%p, !!! popped traced function %s, expected %s\n",
                  tid(), __func__, traced_function_stack,  popped_func, funcname);
            fprintf(stderr, "Current traced function stack:\n");
            debug_traced_function_stack(true);
         }
      }
   }
}


void free_traced_function_stack() {
   if (traced_function_stack) {
      printf("[%d](free_traced_function_stack) traced_function_stack=%p. Executing.\n",
            tid(), traced_function_stack);
      printf("[%d](free_traced_function_stack) Final contents of traced_function_stack:\n", tid());
      debug_traced_function_stack(true);
      g_queue_free_full(traced_function_stack, g_free);
   }
}

