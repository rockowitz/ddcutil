/** @file core.h
 * Core functions and global variables.
 *
 * File core.c provides a collection of inter-dependent services at the core
 * of the **ddcutil** application.
 *
 * These include
 * - message destination redirection
 * - abnormal termination
 * - standard function call options
 * - timestamp generation
 * - message level control
 * - debug and trace messages
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef BASE_CORE_H_
#define BASE_CORE_H_

#include "config.h"

/** \cond */
#ifdef TARGET_BSD
#else
#include <linux/limits.h>
#endif
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
/** \endcond */

#include "public/ddcutil_types.h"

#include "util/coredefs.h"
#include "util/error_info.h"

#include "base/core_per_thread_settings.h"

//
// Common macros
//

#define ASSERT_MARKER(_struct_ptr, _marker_value) \
   assert(_struct_ptr && memcmp(_struct_ptr->marker, _marker_value, 4) == 0)


//
// Standard function call arguments and return values
//

/** Byte of standard call options */
typedef Byte Call_Options;
#define CALLOPT_NONE         0x00    ///< no options
#define CALLOPT_ERR_MSG      0x80    ///< issue message if error
// #define CALLOPT_ERR_ABORT    0x40    ///< terminate execution if error
#define CALLOPT_RDONLY       0x20    ///< open read-only
#define CALLOPT_WARN_FINDEX  0x10    ///< issue warning msg re hiddev_field_info.field_index change
#define CALLOPT_FORCE        0x08    ///< ignore various validity checks
#define CALLOPT_WAIT         0x04    ///< wait on locked resources, if false then fail

char * interpret_call_options_t(Call_Options calloptions);

#ifdef FUTURE
// A way to return both a status code and a pointer.
// Has the benefit of avoiding the "Something** parm" construct,
// but requires a cast by the caller, so loses type checking.
// How useful will this be?

typedef struct {
   int      rc;
   void *   result;
} RC_And_Result;
#endif


//
// Trace message control
//

extern bool dbgtrc_show_time;       // prefix debug/trace messages with elapsed time
extern bool dbgtrc_show_wall_time;  // prefix debug/trace messages with wall time
extern bool dbgtrc_show_thread_id;  // prefix debug/trace messages with thread id

void set_libddcutil_output_destination(const char * filename, const char * traced_unit);
void add_traced_function(const char * funcname);
bool is_traced_function( const char * funcname);
void show_traced_functions();

void add_traced_file(const char * filename);
bool is_traced_file( const char * filename);
void show_traced_files();

DDCA_Trace_Group trace_class_name_to_value(char * name);
void set_trace_groups(DDCA_Trace_Group trace_flags);
void add_trace_groups(DDCA_Trace_Group trace_flags);
// char * get_active_trace_group_names();  // unimplemented
void show_trace_groups();

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
//  Error_Info reporting
//

extern bool report_freed_exceptions;


//
// DDC data error reporting
//

// Controls display of messages regarding I2C error conditions that can be retried.
// extern bool report_ddc_errors;

// thread specific
bool enable_report_ddc_errors(bool onoff);
bool is_report_ddc_errors_enabled();


bool is_reporting_ddc(DDCA_Trace_Group trace_group, const char * filename, const char * funcname);
#define IS_REPORTING_DDC() is_reporting_ddc(TRACE_GROUP, __FILE__, __func__)

bool ddcmsg(DDCA_Trace_Group trace_group, const char* funcname, const int lineno, const char* fn, char* format, ...);
#define DDCMSG0(format, ...) ddcmsg(TRACE_GROUP, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

/** Variant of **DDCMSG** that takes an explicit trace group as an argument.
 *
 * @param debug_flag
 * @param trace_group
 * @param format
 * @param ...
 */
#define DDCMSGX(debug_flag, trace_group, format, ...) \
   ddcmsg(( (debug_flag) ) ? 0xff : (trace_group), __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


/** Macro that wrappers function **ddcmsg()**, passing the current TRACE_GROUP,
 *  file name, line number, and function name as arguments.
 *
 * @param debug_flag
 * @param format
 * @param ...
 */
#define DDCMSG(debug_flag, format, ...) \
   ddcmsg(( (debug_flag) ) ? 0xff : (TRACE_GROUP), __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


// Show report levels for all types
void show_reporting();

// report ddcutil version
void show_ddcutil_version();


//
// Issue messages of various types
//

void severemsg(
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);

extern bool trace_to_syslog;

bool dbgtrc(
        DDCA_Trace_Group  trace_group,
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);

bool dbgtrc_returning(
        DDCA_Trace_Group  trace_group,
        const char * funcname,
        const int    lineno,
        const char * fn,
        int          rc,
        char *       format,
        ...);

// n. using ___LINE__ instead of line in _assert_fail() causes compilation error
#define TRACED_ASSERT(_assertion) \
   do { \
      if (_assertion) { \
         ;              \
      }                 \
      else {           \
         int line = __LINE__;  \
         dbgtrc(DDCA_TRC_ALL, __func__, __LINE__, __FILE__,   \
                      "Assertion failed: \"%s\" in file %s at line %d",  \
                      #_assertion, __FILE__,  __LINE__);   \
         syslog(LOG_ERR, "Assertion failed: \"%s\" in file %s at line %d",  \
                         #_assertion, __FILE__,  __LINE__);   \
         __assert_fail(#_assertion, __FILE__, line, __func__); \
      } \
   } while (0)


#ifndef TRACED_ASSERT_IFF
#define TRACED_ASSERT_IFF(_cond1, _cond2) \
   TRACED_ASSERT( ( (_cond1) && (_cond2) ) || ( !(_cond1) && !(_cond2) ) )
#endif

#define TRACED_ABORT(_assertion) \
   do { \
      int line = __LINE__;  \
      dbgtrc(true, __func__, __LINE__, __FILE__,   \
                   "Assertion failed: \"%s\" in file %s at line %d",  \
                   #_assertion, __FILE__,  __LINE__);   \
      syslog(LOG_ERR, "Assertion failed: \"%s\" in file %s at line %d",  \
                      #_assertion, __FILE__,  __LINE__);   \
      __assert_fail(#_assertion, __FILE__, line, __func__); \
   } while (0)



#define SEVEREMSG(          format, ...) \
   severemsg(          __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

// cannot map to dbgtrc, writes to stderr, not stdout
// #define SEVEREMSG(format, ...) dbgtrc(DDCA_TRC_ALL,       __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

#define DBGMSG(            format, ...) dbgtrc(DDCA_TRC_ALL, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

#define DBGMSF(debug_flag, format, ...) \
   do { if (debug_flag) dbgtrc(DDCA_TRC_ALL, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__); }  while(0)

// For messages that are issued either if tracing is enabled for the appropriate trace group or
// if a debug flag is set.
#define DBGTRC(debug_flag, trace_group, format, ...) \
    dbgtrc( ( (debug_flag) ) ? DDCA_TRC_ALL : (trace_group), __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

#define DBGTRC_STARTING(debug_flag, trace_group, format, ...) \
    dbgtrc( ( (debug_flag) ) ? DDCA_TRC_ALL : (trace_group), __func__, __LINE__, __FILE__, "Starting  "format, ##__VA_ARGS__)

#define DBGTRC_DONE(debug_flag, trace_group, format, ...) \
    dbgtrc( ( (debug_flag) ) ? DDCA_TRC_ALL : (trace_group), __func__, __LINE__, __FILE__, "Done      "format, ##__VA_ARGS__)

#define DBGTRC_NOPREFIX(debug_flag, trace_group, format, ...) \
    dbgtrc( ( (debug_flag) ) ? DDCA_TRC_ALL : (trace_group), __func__, __LINE__, __FILE__, "          "format, ##__VA_ARGS__)

#define DBGTRC_RETURNING(debug_flag, trace_group, rc, format, ...) \
    dbgtrc_returning( ( (debug_flag) ) ? DDCA_TRC_ALL : (trace_group), __func__, __LINE__, __FILE__, rc, format, ##__VA_ARGS__)

// typedef (*dbg_struct_func)(void * structptr, int depth);
#define DBG_RET_STRUCT(_flag, _structname, _dbgfunc, _structptr) \
if (_flag) { \
   dbgtrc(DDCA_TRC_ALL, __func__, __LINE__, __FILE__, "Returning %s at %p", #_structname, _structptr); \
   if (_structptr) { \
      _dbgfunc(_structptr, 1); \
   } \
}

#define DBGTRC_RET_STRUCT(_flag, _trace_group, _structname, _dbgfunc, _structptr) \
if ( (_flag) || (is_tracing(_trace_group, __FILE__, __func__)) ) { \
   dbgtrc(DDCA_TRC_ALL, __func__, __LINE__, __FILE__, "Returning %s at %p", #_structname, _structptr); \
   if (_structptr) { \
      _dbgfunc(_structptr, 1); \
   } \
}


//
// Error handling
//

void report_ioctl_error(
      const char * ioctl_name,
      int          errnum,
      const char * funcname,
      const char * filename,
      int          lineno);

#define REPORT_IOCTL_ERROR(_ioctl_name, _errnum) \
   report_ioctl_error(_ioctl_name, _errnum, __func__, __FILE__, __LINE__);

// reports a program logic error
void program_logic_error(
      const char * funcname,
      const int    lineno,
      const char * fn,
      char *       format,
      ...);

/** @def PROGRAM_LOGIC_ERROR(format,...)
 *  Wraps call to program_logic_error()
 *
 *  Reports an error in program logic.
 * @ingroup abnormal_termination
 */
#define PROGRAM_LOGIC_ERROR(format, ...) \
   program_logic_error(__func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

void set_default_thread_output_settings(FILE * fout, FILE * ferr);
void init_core();

#endif /* BASE_CORE_H_ */
