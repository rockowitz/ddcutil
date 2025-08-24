/** @file core.h
 * Core functions and global variables.
 *
 * File core.c provides a collection of inter-dependent services at the core
 * of the **ddcutil** application.
 *
 * These include
 *  - standard function call options
 *  - debug and trace messages
 *  - abnormal termination
 */

// Copyright (C) 2014-2025 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "util/common_inlines.h"
#include "util/common_printf_formats.h"
#include "util/coredefs.h"
#include "util/error_info.h"
#include "util/failsim.h"
#include "util/msg_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/traced_function_stack.h"

#include "base/parms.h"      // ensure available to any file that includes core.h
#include "base/core_per_thread_settings.h"
#include "base/linux_errno.h"
#include "base/status_code_mgt.h"
#include "base/trace_control.h"  // so don't need to repeatedly include trace_control.h

//
// Common macros
//

#define ASSERT_MARKER(_struct_ptr, _marker_value) \
   assert(_struct_ptr && memcmp(_struct_ptr->marker, _marker_value, 4) == 0)


//
// Global Variables
// 

// Indicates that all tracing facilities have been configured:
extern bool tracing_initialized;
extern bool library_disabled;
extern bool timestamp_in_syslog_debug_msgs;
extern bool running_as_root;


//
// Standard function call arguments and return values
//

/** Byte of standard call options */
typedef Byte Call_Options;
#define CALLOPT_NONE         0x00    ///< no options
#define CALLOPT_ERR_MSG      0x80    ///< issue message if error
#define CALLOPT_RDONLY       0x20    ///< open read-only
#define CALLOPT_WARN_FINDEX  0x10    ///< issue warning msg re hiddev_field_info.field_index change
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

extern __thread  int  trace_api_call_depth;
extern __thread  unsigned int  trace_callstack_call_depth;
// extern __thread  char * trace_callstack[100];


// void set_libddcutil_output_destination(const char * filename, const char * traced_unit);

typedef uint16_t Dbgtrc_Options;
#define DBGTRC_OPTIONS_NONE      0x00
#define DBGTRC_OPTIONS_SYSLOG    0x01
#define DBGTRC_OPTIONS_SEVERE    0x02
#define DBGTRC_OPTIONS_API_CALL  0x04   // used for tracing API
#define DBGTRC_OPTIONS_STARTING  0x08
#define DBGTRC_OPTIONS_DONE      0x10

bool is_tracing(DDCA_Trace_Group trace_group, const char * filename, const char * funcname);
bool is_backtracing(const char * funcname);

//
//  Error_Info reporting
//

extern bool report_freed_exceptions;


//
// DDC data error reporting
//

// Controls display of messages regarding I2C error conditions that can be retried.
// Applies to all threads.
bool enable_report_ddc_errors(bool onoff);  // thread safe

bool is_report_ddc_errors_enabled();


bool is_reporting_ddc(
      DDCA_Trace_Group trace_group,
      const char *     filename,
      const char *     funcname);
#define IS_REPORTING_DDC() is_reporting_ddc(TRACE_GROUP, __FILE__, __func__)

bool ddcmsg(
      DDCA_Trace_Group trace_group,
      const char*      funcname,
      const int        lineno,
      const char*      filename,
      char*            format,
      ...);

/** Variant of **DDCMSG** that takes an explicit trace group as an argument.
 *
 * @param debug_flag
 * @param trace_group
 * @param format
 * @param ...
 */
#define DDCMSGX(debug_flag, trace_group, format, ...) \
   ddcmsg(( (debug_flag) ) ? 0xff : (trace_group), \
            __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

/** Macro that wrappers function ddcmsg(), passing the current TRACE_GROUP,
 *  file name, line number, and function name as arguments.
 *
 * @param debug_flag
 * @param format
 * @param ...
 */
#define DDCMSG(debug_flag, format, ...) \
   ddcmsg(( (debug_flag) ) ? 0xff : (TRACE_GROUP), \
            __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

bool logable_msg(DDCA_Syslog_Level log_level,
            const char * funcname,
            const int    lineno,
            const char * filename,
            char *       format,
            ...);

#define LOGABLE_MSG(importance, format, ...) \
   logable_msg(importance, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


// Show report levels for all types
void show_reporting();

// report ddcutil version
void show_ddcutil_version();


//
// Issue messages of various types
//

bool dbgtrc(
        DDCA_Trace_Group trace_group,
        Dbgtrc_Options   options,
        const char *     funcname,
        const int        lineno,
        const char *     fn,
        char *           format,
        ...);

bool dbgtrc_ret_ddcrc(
        DDCA_Trace_Group trace_group,
        Dbgtrc_Options   options,
        const char *     funcname,
        const int        lineno,
        const char *     fn,
        int              rc,
        char *           format,
        ...);

#ifdef UNNECESSARY
// use ddbgtrc_returning_string()
bool dbgtrc_ret_bool(
        DDCA_Trace_Group trace_group,
        Dbgtrc_Options   options,
        const char *     funcname,
        const int        lineno,
        const char *     fn,
        bool             result,
        char *           format,
        ...);
#endif

bool dbgtrc_returning_errinfo(
        DDCA_Trace_Group trace_group,
        Dbgtrc_Options   options,
        const char *     funcname,
        const int        lineno,
        const char *     fn,
        Error_Info *     errs,
        char *           format,
        ...);

bool dbgtrc_returning_string(
        DDCA_Trace_Group trace_group,
        Dbgtrc_Options   options,
        const char *     funcname,
        const int        lineno,
        const char *     fn,
        const char *     retval_expression,
        char *           format,
        ...);

/* __assert_fail() is not part of the C spec, it is part of the Linux
 * implementation of assert(), etc., which are macros.
 * It is reported to not exist on Termux.
 * However, if  "assert(#_assertion);" is used instead of "__assert_fail(...);",
 * the program does not terminate.
 * 05/2025:
 * Using exit(1) to terminate apparently causes powerdevil to terminate with status
 * success, and as a result it does not automatically restart.
 * Revert to using __assert_fail() to terminate.
 * tmux source itself does not reference ddcutil, so it's unclear what the
 * compatibility issue is.  Failure to build in obscure environments where
 * __assert_fail() does not exist is less important than powerdevil/kde integration.
 */
// n. using ___LINE__ instead of line in __assert_fail() causes compilation error
#ifdef NDEBUG
#define TRACED_ASSERT(_assertion) do {} while (0)
#define TRACED_ASSERT_IFF(_cond1,_cond2) do {} while (0)
#else
#define TRACED_ASSERT(_assertion) \
   do { \
         if (_assertion) { \
         ;              \
      }                 \
      else {           \
         int line = __LINE__;  \
         dbgtrc(DDCA_TRC_ALL, DBGTRC_OPTIONS_NONE, __func__, __LINE__, __FILE__,   \
                      "Assertion failed: \"%s\" in file %s at line %d",  \
                      #_assertion, __FILE__,  __LINE__);   \
         SYSLOG2(DDCA_SYSLOG_ERROR, "Assertion failed: \"%s\" in file %s at line %d",  \
                         #_assertion, __FILE__,  __LINE__);   \
         /* assert(#_assertion); */ \
         __assert_fail(#_assertion, __FILE__, line, __func__);  \
         /* don't need assertion info, dbgtrc() and dbgtrc() have been called */ \
         /* exit(1); */  \
      } \
   } while (0)

#define TRACED_ASSERT_IFF(_cond1, _cond2) \
   TRACED_ASSERT( ( (_cond1) && (_cond2) ) || ( !(_cond1) && !(_cond2) ) )
#endif

#ifdef UNUSED
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
#endif


#define CLOSE_W_ERRMSG(_fd) \
   do \
   { \
      int close_rc = close(fd); \
      if (close_rc < 0) { \
         dbgtrc(DDCA_TRC_ALL, DBGTRC_OPTIONS_SEVERE, __func__, __LINE__, __FILE__, \
               "Unexpected error on close(): fd=%d, filename=%s, errno=%s at line %d in file %s", \
               _fd, filename_for_fd_t(_fd), linux_errno_name(errno), __LINE__, __FILE__); \
      } \
   } while(0)


#define SEVEREMSG(format, ...) \
   dbgtrc(DDCA_TRC_ALL, DBGTRC_OPTIONS_SEVERE, \
          __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

#define DBGMSG(            format, ...) \
   dbgtrc(DDCA_TRC_ALL, DBGTRC_OPTIONS_NONE, \
          __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

#define DBGMSF(debug_flag, format, ...) \
   do { if (debug_flag) dbgtrc(DDCA_TRC_ALL, DBGTRC_OPTIONS_NONE, \
        __func__, __LINE__, __FILE__, format, ##__VA_ARGS__); }  while(0)

// For messages that are issued either if tracing is enabled for the appropriate trace group or
// if a debug flag is set.
#define DBGTRC(debug_flag, trace_group, format, ...) \
    dbgtrc( (debug_flag) ? DDCA_TRC_ALL : (trace_group), DBGTRC_OPTIONS_NONE, \
            __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

#ifdef UNUSED
#define DBGTRC_SYSLOG(debug_flag, trace_group, format, ...) \
    dbgtrc( (debug_flag) ? DDCA_TRC_ALL : (trace_group), DBGTRC_OPTIONS_SYSLOG, \
            __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#endif

#define DBGTRC_STARTING(debug_flag, trace_group, format, ...) \
   do { \
      if (traced_function_stack_enabled) \
         push_traced_function(__func__); \
      dbgtrc( (debug_flag) || trace_callstack_call_depth > 0 || is_traced_callstack_call(__func__) \
                 ? DDCA_TRC_ALL : (trace_group), \
              DBGTRC_OPTIONS_STARTING, \
              __func__, __LINE__, __FILE__, "Starting  "format, ##__VA_ARGS__); \
   } while(0)

#define DBGTRC_DONE(debug_flag , trace_group, format, ...) \
   do { \
      dbgtrc( (debug_flag) || trace_callstack_call_depth > 0  \
              ? DDCA_TRC_ALL : (trace_group), DBGTRC_OPTIONS_DONE, \
              __func__, __LINE__, __FILE__, "Done      "format, ##__VA_ARGS__); \
      if (traced_function_stack_enabled) \
         pop_traced_function(__func__);  \
   } while (0)

#define DBGTRC_DONE_WO_TRACED_FUNCTION_STACK(debug_flag , trace_group, format, ...) \
   do { \
      dbgtrc( (debug_flag) || trace_callstack_call_depth > 0  \
              ? DDCA_TRC_ALL : (trace_group), DBGTRC_OPTIONS_DONE, \
              __func__, __LINE__, __FILE__, "Done      "format, ##__VA_ARGS__); \
   } while (0)

#define DBGTRC_EXECUTED(debug_flag, trace_group, format, ...) \
    dbgtrc( (debug_flag) || trace_callstack_call_depth > 0  ? DDCA_TRC_ALL : (trace_group), DBGTRC_OPTIONS_STARTING | DBGTRC_OPTIONS_DONE, \
            __func__, __LINE__, __FILE__, "Executed  "format, ##__VA_ARGS__)

#define DBGTRC_NOPREFIX(debug_flag, trace_group, format, ...) \
    dbgtrc( (debug_flag) || trace_callstack_call_depth > 0  ? DDCA_TRC_ALL : (trace_group), DBGTRC_OPTIONS_NONE, \
            __func__, __LINE__, __FILE__, "          "format, ##__VA_ARGS__)

#define DBGTRC_RET_STRING(debug_flag, trace_group, _result, format, ...) \
    do { \
       dbgtrc_returning_string( \
          (debug_flag) || trace_callstack_call_depth > 0  \
             ? DDCA_TRC_ALL : (trace_group), \
          DBGTRC_OPTIONS_DONE, \
          __func__, __LINE__, __FILE__, _result, format, ##__VA_ARGS__); \
       if (traced_function_stack_enabled) \
          pop_traced_function(__func__);  \
    } while (0)

/* Notes on macros that have ENABLE_FAILSIM variants.
 *
 * Care must be taken with the DBGTRC_ macros that have ENABLE_FAILSIM variants.
 *
 * - The macros are passed the name of a variable that may be modified by a
 *   failure simulation function. Therefore, what is specified in the return
 *   value field of these macros must be a simple variable that can be the
 *   lvalue of an assignment, not an expression or a constant.
 *
 * - DBGTRC__2() variants of the macros specify an lvalue expression that is to be
 *   set to NULL if an error is injected.  The supports the common case
 *   where the return code is 0 or NULL iff the lvalue is non-null.
 */
#ifdef ENABLE_FAILSIM
#define DBGTRC_RET_DDCRC(debug_flag, trace_group, rc, format, ...) \
   do { \
      if (failsim_enabled && rc != 0) { \
         int injected = fsim_int_injector(rc, __FILE__, __func__); \
         if (injected) { \
            rc = injected; \
            printf("(%s) failsim: injected error %s\n", __func__, psc_desc(rc)); \
         } \
      } \
      dbgtrc_ret_ddcrc( \
         (debug_flag) || trace_callstack_call_depth > 0  ? DDCA_TRC_ALL : (trace_group), DBGTRC_OPTIONS_DONE, \
         __func__, __LINE__, __FILE__, rc, format, ##__VA_ARGS__); \
      if (traced_function_stack_enabled) \
         pop_traced_function(__func__);
  } while (0)
#define DBGTRC_RET_DDCRC2(debug_flag, trace_group, _rc, _data_pointer, format, ...) \
   do { \
      if (failsim_enabled && _rc == 0) { \
         int injected = fsim_int_injector(_rc, __FILE__, __func__); \
         if (injected) { \
            _rc = injected; \
            printf("(%s) failsim: injected error %s, setting %s = NULL\n", __func__, psc_desc(_rc), #_data_pointer); \
            if (_data_pointer) { \
               free(_data_pointer); \
               _data_pointer = NULL; \
            } \
         } \
      } \
      dbgtrc_ret_ddcrc( \
         (debug_flag) || trace_callstack_call_depth > 0  ? DDCA_TRC_ALL : (trace_group), DBGTRC_OPTIONS_DONE, \
         __func__, __LINE__, __FILE__, _rc, format, ##__VA_ARGS__); \
      if (traced_function_stack_enabled) \
         pop_traced_function(__func__);
  } while (0)
#else
#define DBGTRC_RET_DDCRC(debug_flag, trace_group, rc, format, ...) \
   do { \
      dbgtrc_ret_ddcrc( \
         (debug_flag) || trace_callstack_call_depth > 0  ? DDCA_TRC_ALL : (trace_group), \
         DBGTRC_OPTIONS_DONE, \
         __func__, __LINE__, __FILE__, rc, format, ##__VA_ARGS__); \
      if (traced_function_stack_enabled) \
         pop_traced_function(__func__);  \
   } while (0)

#define DBGTRC_RET_DDCRC2(debug_flag, trace_group, rc, data_pointer, format, ...) \
      do { \
   dbgtrc_ret_ddcrc( \
      (debug_flag) || trace_callstack_call_depth > 0  ? DDCA_TRC_ALL : (trace_group), DBGTRC_OPTIONS_DONE, \
      __func__, __LINE__, __FILE__, rc, format, ##__VA_ARGS__); \
      if (traced_function_stack_enabled) \
         pop_traced_function(__func__);  \
   } while (0)
#endif

#ifdef ENABLE_FAILSIM
#define DBGTRC_RET_ERRINFO(debug_flag, trace_group, errinfo_result, format, ...) \
   do { \
      if (failsim_enabled && !errinfo_result) { \
         Error_Info * injected = fsim_errinfo_injector(errinfo_result, __FILE__, __func__); \
         if (injected) { \
            errinfo_result = injected; \
            printf("(%s) Injected error %s\n", __func__, errinfo_summary(injected)); \
         } \
      } \
      dbgtrc_returning_errinfo( \
          (debug_flag) || trace_callstack_call_depth > 0  ? DDCA_TRC_ALL : (trace_group), DBGTRC_OPTIONS_DONE, \
          __func__, __LINE__, __FILE__, errinfo_result, format, ##__VA_ARGS__); \
      if (traced_function_stack_enabled) \
         pop_traced_function(__func__); \
   } while(0)
#define DBGTRC_RET_ERRINFO2(debug_flag, trace_group, errinfo_result, data_pointer, format, ...) \
   do { \
      if (failsim_enabled && !errinfo_result) { \
         Error_Info * injected = fsim_errinfo_injector(errinfo_result, __FILE__, __func__); \
         if (injected) { \
            errinfo_result = injected; \
            if (data_pointer) { \
               free(data_pointer); \
               data_pointer = NULL; \
            } \
            printf("(%s) Injected error %s, setting %s = NULL\n", __func__, errinfo_summary(injected), #data_pointer); \
         } \
      } \
      dbgtrc_returning_errinfo( \
          (debug_flag) || trace_callstack_call_depth > 0  ? DDCA_TRC_ALL : (trace_group), DBGTRC_OPTIONS_DONE, \
          __func__, __LINE__, __FILE__, errinfo_result, format, ##__VA_ARGS__); \
      if (traced_function_stack_enabled) \
         pop_traced_function(__func__); \
   } while(0)
#else
#define DBGTRC_RET_ERRINFO(debug_flag, trace_group, errinfo_result, format, ...) \
   do { \
      dbgtrc_returning_errinfo( \
             (debug_flag) || trace_callstack_call_depth > 0  ? DDCA_TRC_ALL : (trace_group), \
             DBGTRC_OPTIONS_DONE, __func__, __LINE__, __FILE__, errinfo_result, format, ##__VA_ARGS__); \
      if (traced_function_stack_enabled) \
         pop_traced_function(__func__);  \
   } while (0)
#define DBGTRC_RET_ERRINFO2(debug_flag, trace_group, errinfo_result, pointer, format, ...) \
      do { \
         dbgtrc_returning_errinfo( \
             (debug_flag) || trace_callstack_call_depth > 0  ? DDCA_TRC_ALL : (trace_group), \
             DBGTRC_OPTIONS_DONE, \
             __func__, __LINE__, __FILE__, errinfo_result, format, ##__VA_ARGS__); \
         if (traced_function_stack_enabled) \
            pop_traced_function(__func__);  \
      } while (0)
#endif

// can only pass a variable, not an expression or constant, to DBGTRC_RET_BOOL()
// because failure simulation may assign a new value to the variable
#define DBGTRC_RET_BOOL(debug_flag, trace_group, bool_result, format, ...) \
   do { \
      dbgtrc_returning_string( \
         (debug_flag) || trace_callstack_call_depth > 0  ? DDCA_TRC_ALL : (trace_group), \
         DBGTRC_OPTIONS_DONE, \
         __func__, __LINE__, __FILE__, SBOOL(bool_result), format, ##__VA_ARGS__); \
     if (traced_function_stack_enabled) \
        pop_traced_function(__func__);  \
  } while (0)

// typedef (*dbg_struct_func)(void * structptr, int depth);
#define DBGMSF_RET_STRUCT(_debug_flag, _structname, _dbgfunc, _structptr) \
   do { \
      if ((_debug_flag) || trace_callstack_call_depth > 0)  { \
         dbgtrc(DDCA_TRC_ALL, DBGTRC_OPTIONS_DONE, __func__, __LINE__, __FILE__, \
                "Returning %s at %p", #_structname, _structptr); \
         if (_structptr) { \
            _dbgfunc(_structptr, 1); \
         } \
      } \
      if (traced_function_stack_enabled) \
         pop_traced_function(__func__);  \
   } while (0)

#define DBGTRC_RET_STRUCT(_flag, _trace_group, _structname, _dbgfunc, _structptr) \
do { \
   if ( (_flag)  || trace_callstack_call_depth > 0 || is_tracing(_trace_group, __FILE__, __func__) )  { \
      dbgtrc(DDCA_TRC_ALL, DBGTRC_OPTIONS_DONE, \
             __func__, __LINE__, __FILE__, \
             "Returning %s at %p", #_structname, _structptr); \
      if (_structptr) { \
         _dbgfunc(_structptr, 1); \
      } \
   } \
   if (traced_function_stack_enabled) \
      pop_traced_function(__func__);  \
} while(0)

#define DBGTRC_RET_STRUCT_VALUE(_flag, _trace_group, _structname, _dbgfunc, _structval) \
do { \
   if ( (_flag)  || trace_callstack_call_depth > 0 || is_tracing(_trace_group, __FILE__, __func__) )  { \
      dbgtrc(DDCA_TRC_ALL, DBGTRC_OPTIONS_DONE, \
             __func__, __LINE__, __FILE__, \
             "Returning %s value:", #_structname); \
      _dbgfunc(_structval, 2); \
   } \
   if (traced_function_stack_enabled) \
      pop_traced_function(__func__);  \
} while(0)


#define DBGTRC_RET_ERRINFO_STRUCT(_debug_flag, _trace_group, _errinfo_result, \
                                  _structptr_loc, _dbgfunc)                   \
do { \
   if ( (_debug_flag || trace_callstack_call_depth > 0 ) || is_tracing(_trace_group, __FILE__, __func__) )  {    \
      dbgtrc_returning_errinfo(DDCA_TRC_ALL, DBGTRC_OPTIONS_DONE,             \
              __func__, __LINE__, __FILE__,                                   \
              _errinfo_result, "*%s = %p", #_structptr_loc, *_structptr_loc); \
      if (*_structptr_loc) {                                                  \
         _dbgfunc(*_structptr_loc, 1);                                        \
      }     \
   } \
   if (traced_function_stack_enabled) \
      pop_traced_function(__func__);  \
} while(0)


//
// Error handling
//

#define REPORT_IOCTL_ERROR(_ioctl_name, _errnum) \
      dbgtrc(DDCA_TRC_ALL, DBGTRC_OPTIONS_SEVERE, __func__, __LINE__, __FILE__, \
             "Error in ioctl(%s), %s", _ioctl_name, linux_errno_desc(_errnum));

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

#ifdef UNUSED
void core_errmsg_emitter(
      GPtrArray*   errmsgs,
      GPtrArray *  errinfo_accum,
      bool         verbose,
      int          rc,
      const char * func,
      const char * msg, ...);
#endif


//
// Use of system log
//

extern bool msg_to_syslog_only;

extern bool enable_syslog;
extern DDCA_Syslog_Level syslog_level;

DDCA_Syslog_Level   syslog_level_name_to_value(const char * name);
const char *        syslog_level_name(DDCA_Syslog_Level level);
bool                test_emit_syslog(DDCA_Syslog_Level msg_level);
int                 syslog_importance_from_ddcutil_syslog_level(DDCA_Syslog_Level level);
extern const char * valid_syslog_levels_string;


/** Checks that the specified ddcutil severity level is at least as great
 *  as the severity cutoff level currently in effect.  If so, the ddcutil
 *  severity is converted to a syslog severity, and the messages is written
 *  to the system log.
 *
 *  @param _ddcutil_severity   e.g. DDCA_SYSLOG_ERROR
 *  @param  fmt                message format
 *  @param  ...                message arguments
 */
#define DECORATED_SYSLOG(_ddcutil_severity, format, ...) \
do { \
   if (test_emit_syslog(_ddcutil_severity)) { \
      int syslog_priority = syslog_importance_from_ddcutil_syslog_level(_ddcutil_severity);  \
      if (syslog_priority >= 0) { \
         char * body = g_strdup_printf(format, ##__VA_ARGS__); \
         char prefix[100] = {0}; \
         if (rpt_get_ornamentation_enabled() ) { \
            get_msg_decoration(prefix, 100, true); \
         } \
         syslog(syslog_priority, "%s%s%s", prefix, body, (tag_output) ? " (N)" : ""  ); \
         free(body); \
      } \
   } \
} while(0)


#define SYSLOG2(_ddcutil_severity, format, ...) \
do { \
   if (test_emit_syslog(_ddcutil_severity)) { \
      int syslog_priority = syslog_importance_from_ddcutil_syslog_level(_ddcutil_severity);  \
      if (syslog_priority >= 0) { \
         char * body = g_strdup_printf(format, ##__VA_ARGS__); \
         syslog(syslog_priority, PRItid" %s%s", (intmax_t) tid(), body, (tag_output) ? " (P)" : "" ); \
         free(body); \
      } \
   } \
} while(0)



/** Writes a message to the current ferr() or fout() device and, depending on
 *  the specified ddcutil severity and current syslog level, to the system log.
 *
 *  @param _ddcutil_severity   e.g. DDCA_SYSLOG_ERROR
 *  @param  fmt                message format
 *  @param  ...                message arguments
 *
 *  Messages with ddcutil severity DDCA_SYSLOG_WARNING or more severe are
 *  written to the ferr() device.  Others are written to the fout() device.
 *
 *  Messages are written to the system log with the syslog priority
 *  corresponding to the ddcutil severity.
 *
 *  If global **msg_to_syslog_only** is set, the message is written only
 *  to the system log, not to the terminal.  This is avoid duplicate messages
 *  in the system log if terminal output is redirected to the system log,
 *  as is the case for KDE applications.
 */
#define MSG_W_SYSLOG(_ddcutil_severity, format, ...) \
do { \
   if (!msg_to_syslog_only) { \
      FILE * f = (_ddcutil_severity <= DDCA_SYSLOG_WARNING) ? ferr() : fout(); \
      fprintf(f, format, ##__VA_ARGS__); \
      fprintf(f, "\n"); \
   } \
   if (test_emit_syslog(_ddcutil_severity)) { \
      int syslog_priority = syslog_importance_from_ddcutil_syslog_level(_ddcutil_severity);  \
      if (syslog_priority >= 0) { \
         char * body = g_strdup_printf(format, ##__VA_ARGS__); \
         syslog(syslog_priority, PRItid" %s%s", (intmax_t) tid(), body, (tag_output) ? " (Q)" : "" ); \
         free(body); \
      } \
   } \
} while(0)


void base_errinfo_free_with_report(
      Error_Info *  erec,
      bool          report,
      const char *  func);

#define BASE_ERRINFO_FREE_WITH_REPORT(_erec, _report) \
   base_errinfo_free_with_report(_erec, (_report), __func__)


//
// Output capture
//

void   start_capture(DDCA_Capture_Option_Flags flags);
char * end_capture(void);
Null_Terminated_String_Array end_capture_as_ntsa();


//
// Initialization
//

void detect_stdout_stderr_redirection();
void init_core();

#endif /* BASE_CORE_H_ */
