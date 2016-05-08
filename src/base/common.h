/* common.h
 *
 * Created on: Jun 5, 2014
 *     Author: rock
 *
 * Declarations used throughout the I2C DDC application.
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


#ifndef DDC_COMMON_H_
#define DDC_COMMON_H_

#include "base/util.h"
#include "base/displays.h"
#include "base/msg_control.h"


//
// Sleep and sleep statistics
//

void sleep_millis( int milliseconds);
void sleep_millis_with_trace(int milliseconds, const char * caller_location, const char * message);

typedef struct {
   long requested_sleep_milliseconds;
   long actual_sleep_nanos;
   int  total_sleep_calls;
} Sleep_Stats;

void init_sleep_stats();
Sleep_Stats * get_sleep_stats();
void report_sleep_stats(int depth);


//
// Error handling
//

void terminate_execution_on_error(
        Trace_Group  trace_group,
        const char * funcname,
        const int    lineno,
        const char * fn,
        char *       format,
        ...);

#define TERMINATE_EXECUTION_ON_ERROR(format, ...) \
   terminate_execution_on_error(TRACE_GROUP, __func__, __LINE__, __FILE__, format, ##__VA_ARGS__)


#endif /* DDC_COMMON_H_ */
