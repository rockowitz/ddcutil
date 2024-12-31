/** @file msg_util.h */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef MSG_UTIL_H_
#define MSG_UTIL_H_

#include <glib-2.0/glib.h>
#include <stdbool.h>


extern bool dbgtrc_show_time;       // prefix debug/trace messages with elapsed time
extern bool dbgtrc_show_wall_time;  // prefix debug/trace messages with wall time
extern bool dbgtrc_show_thread_id;  // prefix debug/trace messages with thread id
extern bool dbgtrc_show_process_id; // prefix debug/trace messages with process id
extern bool dbgtrc_trace_to_syslog_only;
extern bool stdout_stderr_redirected;

extern bool dbgtrc_dest_syslog;
extern bool traced_function_stack_enabled;
extern __thread bool msg_decoration_suspended;
extern __thread bool traced_function_stack_suspended;

char*      get_msg_decoration(char * buf, uint bufsize, bool dest_syslog);
char*      formatted_wall_time();
void       push_traced_function(const char * funcname);
char*      peek_traced_function();
void       pop_traced_function(const char * funcname);
void       debug_traced_function_stack(bool reverse);
GPtrArray* get_traced_callstack(bool most_recent_last);
void       free_traced_function_stack();

#endif /* MSG_UTIL_H_ */
