/* core.h
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

/** @file core.h
 */

#ifndef BASE_CORE_H_
#define BASE_CORE_H_

/** \cond */
#include <linux/limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
/** \endcond */

#include "public/ddcutil_types.h"

#include "util/coredefs.h"


// /** @addtogroup abnormal_termination
//  */

//
// Initialization
//
void init_msg_control();
extern bool dbgtrc_show_time;  // include elapsed time in debug/trace timestamps

//
// For aborting out of shared library
//
void register_jmp_buf(jmp_buf* jb);

void ddc_abort(
      const char * funcname,
      const int    lineno,
      const char * fn,
      int          status);

#define DDC_ABORT(status) \
   ddc_abort(__func__, __LINE__, __FILE__, status)

extern DDCA_Global_Failure_Information global_failure_information;


//
// Standard flags to indicate behavior if a system call fails
//

/** Byte of standard call options */
typedef Byte Call_Options;
#define CALLOPT_NONE         0x00    ///< no options
#define CALLOPT_ERR_MSG      0x80    ///< issue message if error
#define CALLOPT_ERR_ABORT    0x40    ///< terminate execution if error
#define CALLOPT_RDONLY       0x20    ///< open read-only
#define CALLOPT_WARN_FINDEX  0x10    ///< issue warning msg re hiddev_field_info.field_index change
#define CALLOPT_FORCE        0x08    ///< ignore various validity checks
// #define CALLOPT_FORCE_SLAVE  0x04    // use ioctl I2C_FORCE_SLAVE

// Return string interpretation of CALLOPT_ flag byte
char * interpret_call_options(Call_Options calloptions);
char * interpret_call_options_t(Call_Options calloptions);


//
// General Function Arguments and Return Values
//

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
// Global redirection for messages that normally go to stdout and stderr,
// used within functions that are part of the shared library.
//
extern FILE * FOUT;
extern FILE * FERR;

void set_fout(FILE * fout);
void set_ferr(FILE * ferr);
void set_fout_to_default();
void set_ferr_to_default();


//
// Message level control
//

DDCA_Output_Level get_output_level();
void              set_output_level(DDCA_Output_Level newval);
char *            output_level_name(DDCA_Output_Level val);


//
// Trace message control
//

void add_traced_function(const char * funcname);
bool is_traced_function( const char * funcname);
void show_traced_functions();

void add_traced_file(const char * filename);
bool is_traced_file( const char * filename);
void show_traced_files();

typedef enum {
 TRC_BASE = 0x80,
 TRC_I2C  = 0x40,
 TRC_ADL  = 0x20,
 TRC_DDC  = 0x10,
 TRC_USB  = 0x08,
 TRC_TOP  = 0x04,

 TRC_NEVER  = 0x00,
 TRC_ALWAYS = 0xff
} Trace_Group;

Trace_Group trace_class_name_to_value(char * name);
void set_trace_levels(Trace_Group trace_flags);
char * get_active_trace_group_names();
void show_trace_groups();


bool is_tracing(Trace_Group trace_group, const char * filename, const char * funcname);
/** Checks if tracking is currently active for the globally defined TRACE_GROUP value,
 *  current file and function.
 *
 *  Wrappers call to **is_tracking()**.
 */
#define IS_TRACING() is_tracing(TRACE_GROUP, __FILE__, __func__)


// Manage DDC data error reporting

// Controls display of messages regarding I2C error conditions that can be retried.
extern bool report_ddc_errors;

bool is_reporting_ddc(Trace_Group trace_group, const char * filename, const char * funcname);
#define IS_REPORTING_DDC() is_reporting_ddc(TRACE_GROUP, __FILE__, __func__)

void ddcmsg(Trace_Group trace_group, const char* funcname, const int lineno, const char* fn, char* format, ...);
#define DDCMSG(format, ...) ddcmsg(TRACE_GROUP, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

#define DDCDBGTRCX(debug_flag, trace_group, format, ...) \
   ddcmsg(( (debug_flag) ) ? 0xff : (trace_group), __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

#define DDCDBGTRC(debug_flag, format, ...) \
   ddcmsg(( (debug_flag) ) ? 0xff : (TRACE_GROUP), __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


// Show report levels for all types
void show_reporting();


//
// Issue messages of various types
//

void severemsg(
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);

bool dbgtrc(
        Trace_Group  trace_group,
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);


#define SEVEREMSG(          format, ...) \
   severemsg(          __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


// cannot map to dbgtrc, writes to stderr, not stdout
// #define SEVEREMSG(format, ...) dbgtrc(0xff,       __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#define DBGMSG(            format, ...) dbgtrc(0xff, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#define DBGMSF(debug_flag, format, ...) \
   do { if (debug_flag) dbgtrc( 0xff, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__); }  while(0)


#define TRCMSG(            format, ...) \
   dbgtrc(TRACE_GROUP, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

// which of these are really useful?
// not currently used: TRCALWAYS, TRCMSGTG, TRCMSGTF
#define TRCALWAYS(            format, ...) \
   dbgtrc(0xff,        __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#define TRCMSGTG(trace_group, format, ...) \
   dbgtrc(trace_group, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#define TRCMSGTF(trace_flag, format, ...) \
    do { if (trace_flag) dbgtrc(0xff, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__); }  while(0)
// alt: dbgtrc( ( (trace_flag) ? (0xff) : TRACE_GROUP ), __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

// For messages that are issued either if tracing is enabled for the appropriate trace group or
// if a debug flag is set.
#define DBGTRC(debug_flag, trace_group, format, ...) \
    dbgtrc( ( (debug_flag) ) ? 0xff : (trace_group), __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


//
// Error handling
//

void report_ioctl_error(
      int         errnum,
      const char* funcname,
      int         lineno,
      char*       filename,
      bool        fatal);

#ifdef UNUSED
void report_ioctl_error2(
      int   errnum,
      int   fh,
      int   request,
      void* data,
      const char* funcname,   // const to avoid warning msg on references at compile time
      int   lineno,
      char* filename,
      bool fatal);
#endif

// reports a program logic error and terminates execution
void program_logic_error(
      const char * funcname,
      const int    lineno,
      const char * fn,
      char *       format,
      ...);

/** @def PROGRAM_LOGIC_ERROR(format,...)
 *  Wraps call to program_logic_error()
 *
 *  Reports an error in program logic and terminates execution.
 * @ingroup abnormal_termination
 */
#define PROGRAM_LOGIC_ERROR(format, ...) \
   program_logic_error(__func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


void terminate_execution_on_error(
        Trace_Group  trace_group,
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);

/** @def TERMINATE_EXECUTION_ON_ERROR(format,...)
 *  Wraps call to terminate_execution_on_error()
 * @ingroup abnormal_termination
 */
#define TERMINATE_EXECUTION_ON_ERROR(format, ...) \
   terminate_execution_on_error(TRACE_GROUP, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

#endif /* BASE_CORE_H_ */
