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

extern __thread  int  trace_api_call_depth;
extern __thread  int  trace_callstack_call_depth;

extern DDCA_Syslog_Level syslog_level;

DDCA_Syslog_Level syslog_level_name_to_value(const char * name);
const char * syslog_level_name(DDCA_Syslog_Level level);
bool test_emit_syslog(DDCA_Syslog_Level msg_level);
int  syslog_importance_from_ddcutil_syslog_level(DDCA_Syslog_Level level);

bool add_traced_function(const char * funcname);
bool is_traced_function( const char * funcname);
void dbgrpt_traced_function_table(int depth);

bool add_traced_api_call(const char * funcname);
bool is_traced_api_call( const char * funcname);

bool add_traced_callstack_call(const char * funcname);
bool is_traced_callstack_call( const char * funcname);

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

//
// Use of system log
//

extern bool trace_to_syslog;
extern bool enable_syslog;
// #define SYSLOG(priority, format, ...) do {if (enable_syslog) syslog(priority, format, ##__VA_ARGS__); } while(0)
// #define SYSLOG(priority, format, ...) do {} while (0)
#define SYSLOG2(_ddcutil_severity, format, ...) \
do { \
   if (test_emit_syslog(_ddcutil_severity)) { \
      int syslog_priority = syslog_importance_from_ddcutil_syslog_level(_ddcutil_severity);  \
      if (syslog_priority >= 0) { \
         syslog(syslog_priority, format, ##__VA_ARGS__); \
      } \
   } \
} while(0)


#endif /* TRACE_CONTROL_H_ */
