/** @file trace_control.h
 *
 *  Manage whether tracing is performed
 */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TRACE_CONTROL_H_
#define TRACE_CONTROL_H_

#include <stdbool.h>

#include "config.h"

#include "public/ddcutil_types.h"

#include "base/ddcutil_types_internal.h"

extern DDCA_Trace_Group trace_levels;

bool add_traced_function(const char * funcname);
bool is_traced_function( const char * funcname);
void dbgrpt_traced_function_table(int depth);

bool add_traced_api_call(const char * funcname);
bool is_traced_api_call( const char * funcname);

bool add_traced_callstack_call(const char * funcname);
bool is_traced_callstack_call( const char * funcname);
void dbgrpt_traced_callstack_call_table(int depth);

void add_traced_file(const char * filename);
bool is_traced_file( const char * filename);

DDCA_Trace_Group trace_class_name_to_value(const char * name);
void set_trace_groups(DDCA_Trace_Group trace_flags);
void add_trace_groups(DDCA_Trace_Group trace_flags);
// char * get_active_trace_group_names();  // unimplemented

void report_tracing(int depth);

bool is_tracing(DDCA_Trace_Group trace_group, const char * filename, const char * funcname);

/** Checks if tracking is currently active for the globally defined TRACE_GROUP value,
 *  current file and function.
 *
 *  Wrappers call to **is_tracing()**, using the current **TRACE_GROUP** value,
 *  filename, and function as implicit arguments.
 */
#define IS_TRACING() is_tracing(TRACE_GROUP, __FILE__, __func__)

#define IS_TRACING_GROUP(grp) is_tracing((grp), __FILE__, __func__)

#define IS_TRACING_BY_FUNC_OR_FILE() is_tracing(DDCA_TRC_NONE, __FILE__, __func__)

#define IS_DBGTRC(debug_flag, group) \
    ( (debug_flag)  || is_tracing((group), __FILE__, __func__) )

#endif /* TRACE_CONTROL_H_ */
