/* execution_stats.h
 *
 * For recording the count and elapsed time of system calls.
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

#ifndef EXECUTION_STATS_H_
#define EXECUTION_STATS_H_

#include <stdbool.h>

#include "base/displays.h"
#include "base/status_code_mgt.h"


// Initialization

void init_execution_stats();


// IO Event Tracking

typedef enum {IE_WRITE, IE_READ, IE_WRITE_READ, IE_OPEN, IE_CLOSE, IE_OTHER} IO_Event_Type;

const char * io_event_name(IO_Event_Type event_type);

void log_io_call(
        const IO_Event_Type  event_type,
        const char *         location,
        long                 start_time_nanos,
        long                 end_time_nanos);

#define RECORD_IO_EVENT(event_type, cmd_to_time)  { \
   long _start_time = cur_realtime_nanosec(); \
   cmd_to_time; \
   log_io_call(event_type, __func__, _start_time, cur_realtime_nanosec()); \
}

void report_io_call_stats(int depth);


// Record Status Code Occurrence

Global_Status_Code log_status_code(Global_Status_Code rc, const char * caller_name);
int log_secondary_status_code(int rc, const char * caller_name);
#define COUNT_STATUS_CODE(rc) log_status_code(rc,__func__)
void show_all_status_counts();


// Sleep Strategy

bool set_sleep_strategy(int strategy);
int get_sleep_strategy();
char * sleep_strategy_desc(int sleep_strategy);

typedef enum {SE_WRITE_TO_READ, SE_POST_OPEN, SE_POST_WRITE, SE_POST_READ } Sleep_Event_Type;
const char * sleep_event_name(Sleep_Event_Type event_type);

// Functions for sleeping.  The actual sleep time is determined
// by the strategy in place given the situation in which sleep is invoked.

// Convenience methods that call call_tuned_sleep():
void call_tuned_sleep_i2c(Sleep_Event_Type event_type);   // DDC_IO_DEVI2C
void call_tuned_sleep_adl(Sleep_Event_Type event_type);   // DDC_IO_ADL
void call_tuned_sleep_dh(Display_Handle* dh, Sleep_Event_Type event_type);
// The workhorse:
void call_tuned_sleep(DDCA_IO_Mode io_mode, Sleep_Event_Type event_type);

void report_sleep_strategy_stats(int depth);

#endif /* EXECUTION_STATS_H_ */
