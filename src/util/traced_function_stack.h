/** @file traced_function_stack.h */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TRACED_FUNCTION_STACK_H_
#define TRACED_FUNCTION_STACK_H_

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <sys/syscall.h>
#include <unistd.h>

extern bool traced_function_stack_enabled;
extern __thread bool traced_function_stack_suspended;

// TODO: merge tid(), pid() with linux_errno.c/h
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

void       push_traced_function(const char * funcname);
char*      peek_traced_function();
void       pop_traced_function(const char * funcname);
void       debug_current_traced_function_stack(bool reverse);
GPtrArray* get_current_traced_function_stack_contents(bool most_recent_last);
void       free_current_traced_function_stack();
void       free_all_traced_function_stacks();


#endif /* TRACED_FUNCTION_STACK_H_ */
