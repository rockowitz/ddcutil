/* util.h
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef I2CPROBE_UTIL
#define I2CPROBE_UTIL

// #include <glib.h>
#include <stdbool.h>
#include <stdlib.h>


//
// General
//

// A way to return both a status code and a pointer.
// Has the benefit of avoiding the "Something** parm" construct,
// but requires a cast by the caller, so loses type checking.
// How useful will this be?

typedef struct {
   int      rc;
   void *   result;
} RC_And_Result;


// For defining boolean "exit if failure" function parameters, allowing
// functions to be called with more comprehensible parameter values than
// "true" and "false".
typedef bool Failure_Action;
static const Failure_Action EXIT_IF_FAILURE = true;
static const Failure_Action RETURN_ERROR_IF_FAILURE = false;


//
// Timing functions
//

// Returns the current value of the realtime clock in nanoseconds:
long cur_realtime_nanosec();
// For debugging:
void report_timestamp_history();


//
// Error handling
//

void report_ioctl_error(
      int         errnum,
      const char* funcname,
      int         lineno,
      char*       filename,
      bool        fatal);

void report_ioctl_error2(
      int   errnum,
      int   fh,
      int   request,
      void* data,
      const char* funcname,   // const to avoid warning msg on references at compile time
      int   lineno,
      char* filename,
      bool fatal);

// reports a program logic error and terminates execution
void program_logic_error(
      const char * funcname,
      const int    lineno,
      const char * fn,
      char *       format,
      ...);

#define PROGRAM_LOGIC_ERROR(format, ...) \
   program_logic_error(__func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

#endif
