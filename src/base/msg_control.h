/* msg_control.h
 *
 * Message management
 *
 * Created on: Jun 15, 2014
 *     Author: rock
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

#ifndef DEBUG_H_
#define DEBUG_H_

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "util/coredefs.h"

#include "base/util.h"


void init_msg_control();

// Global redirection for messages that normally go to stdout and stderr,
// used for API

extern FILE * FOUT;
extern FILE * FERR;

void set_fout(FILE * fout);
void set_ferr(FILE * ferr);


// Message level control

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


// Manage reporting of DDC data errors

// Display messages for I2C error conditions that can be retried.
extern bool show_recoverable_errors;

bool is_reporting_ddc(Trace_Group trace_group, const char * fn);
#define IS_REPORTING_DDC() is_reporting_ddc(TRACE_GROUP, __FILE__)

void ddcmsg(Trace_Group trace_group, const char * funcname, const int lineno, const char * fn, char * format, ...);

#define DDCMSG(format, ...) ddcmsg(TRACE_GROUP, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


// Show report levels for all types

void show_reporting();


// Issue messages of various types

void severemsg(
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);
void dbgmsg(
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);
void trcmsg(
        Trace_Group  trace_group,
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);

#define SEVEREMSG(            format, ...) dbgmsg(             __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#define DBGMSG(               format, ...) dbgmsg(             __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#define DBGMSGF( debug_flag,     format, ...) \
   do { if (debug_flag) dbgmsg(  __func__, __LINE__, __FILE__, format, ##__VA_ARGS__); }  while(0)
#define DBGMSF( debug_flag,     format, ...) \
   do { if (debug_flag) dbgmsg(  __func__, __LINE__, __FILE__, format, ##__VA_ARGS__); }  while(0)
#define TRCMSG(               format, ...) trcmsg(TRACE_GROUP, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
// which of these are really useful?
#define TRCALWAYS(            format, ...) trcmsg(0xff,        __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#define TRCMSGTG(trace_group, format, ...) trcmsg(trace_group, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)
#define TRCMSGTF(trace_flag, format, ...) \
    do { if (trace_flag) trcmsg(0xff, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__); }  while(0)
// alt: trcmsg( ( (trace_flag) ? (0xff) : TRACE_GROUP ), __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)

#endif /* DEBUG_H_ */
