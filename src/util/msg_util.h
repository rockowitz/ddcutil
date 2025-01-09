/** @file msg_util.h */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef MSG_UTIL_H_
#define MSG_UTIL_H_

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <sys/syscall.h>
#include <unistd.h>

extern bool dbgtrc_show_time;       // prefix debug/trace messages with elapsed time
extern bool dbgtrc_show_wall_time;  // prefix debug/trace messages with wall time
extern bool dbgtrc_show_thread_id;  // prefix debug/trace messages with thread id
extern bool dbgtrc_show_process_id; // prefix debug/trace messages with process id
extern bool dbgtrc_trace_to_syslog_only;
extern bool stdout_stderr_redirected;

extern bool traced_function_stack_enabled;
extern __thread bool msg_decoration_suspended;
extern __thread bool traced_function_stack_suspended;
extern __thread pid_t process_id;
extern __thread pid_t thread_id;

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

char*      get_msg_decoration(char * buf, uint bufsize, bool dest_syslog);
char*      formatted_wall_time();
void       push_traced_function(const char * funcname);
char*      peek_traced_function();
void       pop_traced_function(const char * funcname);
void       debug_current_traced_function_stack(bool reverse);
GPtrArray* get_traced_function_stack(bool most_recent_last);
void       free_current_traced_function_stack();
void       free_all_traced_function_stacks();

#endif /* MSG_UTIL_H_ */
