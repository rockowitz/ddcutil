/** @file traced_function_stack.h */

// Copyright (C) 2024-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TRACED_FUNCTION_STACK_H_
#define TRACED_FUNCTION_STACK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <glib-2.0/glib.h>
#include <stdbool.h>

extern bool traced_function_stack_enabled;
extern bool traced_function_stack_errors_fatal;
extern __thread GQueue * traced_function_stack;

bool       set_debug_thread_tfs(bool newval);

void       push_traced_function(const char * funcname);
char*      peek_traced_function();
void       pop_traced_function(const char * funcname);

GPtrArray* get_current_traced_function_stack_contents(bool most_recent_last);

// Symbolic value for the **revers** arguments
#define TFS_MOST_RECENT_FIRST  false
#define TFS_MOST_RECENT_LAST   true
void       collect_traced_function_stack(GPtrArray* collector, GQueue * stack, bool reverse, int stack_adjust);
void       dbgrpt_current_traced_function_stack(bool reverse, bool show_tid, int depth);
int        current_traced_function_stack_size();
void       current_traced_function_stack_to_syslog(int syslog_priority, bool reverse);

GPtrArray* stash_current_traced_function_stack();
void       restore_current_traced_function_stack(GPtrArray* stashed);

void       reset_current_traced_function_stack();
void       free_current_traced_function_stack();
void       free_all_traced_function_stacks();

#ifdef __cplusplus
}    // extern "C"
#endif

#endif /* TRACED_FUNCTION_STACK_H_ */
