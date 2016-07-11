/* core.h
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef BASE_CORE_H_
#define BASE_CORE_H_

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "util/coredefs.h"


//
// Initialization
//
void init_msg_control();
extern bool dbgtrc_show_time;  // include elapsed time in debug/trace timestamps

//
// For aborting out of shared library
//
void register_jmp_buf(jmp_buf* jb);
void ddc_abort(int status);

//
// Generic data structure and function for creating string representation of named bits
//
typedef struct {
   unsigned int  bitvalue;
   char *        bitname;
} Bitname_Table_Entry;

// last entry MUST have bitvalue = 0
typedef Bitname_Table_Entry Bitname_Table[];

char * bitflags_to_string(
          int            flags_val,
          Bitname_Table  bitname_table,
          char *         sepstr,
          char *         buffer,
          int            bufsize );

//
// Standard flags to indicate behavior if a system call fails
//
#define CALLOPT_NONE      0x00
#define CALLOPT_ERR_MSG   0x80      // issue message
#define CALLOPT_ERR_ABORT 0x40      // terminate execution
#define CALLOPT_RDONLY    0x20      // open read-only
#define CALLOPT_WARN_FINDEX 0x01    // issue warning msg re hiddev_field_info.field_index change

// Return string interpretation of CALLOPT_ flag byte
char * interpret_calloptions_r(Byte calloptions, char * buffer, int bufsize);
char * interpret_calloptions(Byte calloptions);

//
// Timestamp Generation
//
long cur_realtime_nanosec();   // Returns the current value of the realtime clock in nanoseconds
void show_timestamp_history(); // For debugging
long elapsed_time_nanaosec();  // nanoseconds since start of program, first call initializes
char * formatted_elapsed_time();


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


// For defining boolean "exit if failure" function arguments, allowing
// functions to be called with more comprehensible parameter values than
// "true" and "false".
typedef bool Failure_Action;
static const Failure_Action EXIT_IF_FAILURE = true;
static const Failure_Action RETURN_ERROR_IF_FAILURE = false;


//
// Global redirection for messages that normally go to stdout and stderr,
// used within functions that are part of the shared library.
//
extern FILE * FOUT;
extern FILE * FERR;

void set_fout(FILE * fout);
void set_ferr(FILE * ferr);


//
// Message level control
//

// Values assigned to constants allow them to be or'd in bit flags
// Values are ascending in order of verbosity, except for OL_DEFAULT
typedef enum {OL_DEFAULT=0x01,
              OL_PROGRAM=0x02,
              OL_TERSE  =0x04,
              OL_NORMAL =0x08,
              OL_VERBOSE=0x10
} Output_Level;
Output_Level get_output_level();
void         set_output_level(Output_Level newval);
char *       output_level_name(Output_Level val);


// Debug trace message control

typedef Byte Trace_Group;
#define TRC_BASE 0x80
#define TRC_I2C  0x40
#define TRC_ADL  0x20
#define TRC_DDC  0x10
#define TRC_USB  0x08
#define TRC_TOP  0x04

#define TRC_NEVER  0x00
#define TRC_ALWAYS 0xff

Trace_Group trace_class_name_to_value(char * name);
void set_trace_levels(Trace_Group trace_flags);
extern const char * trace_group_names[];
extern const int    trace_group_ct;

void show_trace_groups();

bool is_tracing(Trace_Group trace_group, const char * filename);
#define IS_TRACING() is_tracing(TRACE_GROUP, __FILE__)


// Manage DDC data error reporting

// Controls display of messages regarding I2C error conditions that can be retried.
extern bool show_recoverable_errors;

bool is_reporting_ddc(Trace_Group trace_group, const char * fn);
#define IS_REPORTING_DDC() is_reporting_ddc(TRACE_GROUP, __FILE__)

void ddcmsg(Trace_Group trace_group, const char* funcname, const int lineno, const char* fn, char* format, ...);
#define DDCMSG(format, ...) ddcmsg(TRACE_GROUP, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


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

#ifdef OLD
void dbgmsg(
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);
#endif
void dbgtrc(
        Trace_Group  trace_group,
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);


#define SEVEREMSG(          format, ...) \
   severemsg(          __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#ifdef OLD
#define DBGMSG(             format, ...) \
   dbgmsg(             __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#define DBGMSF(debug_flag,  format, ...) \
   do { if (debug_flag) dbgmsg(  __func__, __LINE__, __FILE__, format, ##__VA_ARGS__); }  while(0)
#endif

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

#define PROGRAM_LOGIC_ERROR(format, ...) \
   program_logic_error(__func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


void terminate_execution_on_error(
        Trace_Group  trace_group,
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);

#define TERMINATE_EXECUTION_ON_ERROR(format, ...) \
   terminate_execution_on_error(TRACE_GROUP, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


#endif /* BASE_CORE_H_ */
