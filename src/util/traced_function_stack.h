/** @file traced_function_stack.h */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TRACED_FUNCTION_STACK_H_
#define TRACED_FUNCTION_STACK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <glib-2.0/glib.h>
#include <stdbool.h>
// #include <sys/syscall.h>
// #include <unistd.h>

#include "common_inlines.h"

extern bool traced_function_stack_enabled;
extern __thread bool traced_function_stack_suspended;

void       push_traced_function(const char * funcname);
char*      peek_traced_function();
void       pop_traced_function(const char * funcname);
void       debug_current_traced_function_stack(bool reverse);
GPtrArray* get_current_traced_function_stack_contents(bool most_recent_last);
void       free_current_traced_function_stack();
void       free_all_traced_function_stacks();

#ifdef __cplusplus
}    // extern "C"
#endif

#endif /* TRACED_FUNCTION_STACK_H_ */
