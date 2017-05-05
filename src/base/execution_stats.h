/* execution_stats.h
 *
 * For recording the count and elapsed time of system calls.
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

/** \file
 * Record execution statistics, namely the count and elapsed time of system calls.
 */

#ifndef EXECUTION_STATS_H_
#define EXECUTION_STATS_H_

/** \cond */
#include <inttypes.h>
#include <stdbool.h>
/** \endcond */

#include "util/timestamp.h"

#include "base/displays.h"
#include "base/status_code_mgt.h"


// Initialization

void init_execution_stats();

void reset_execution_stats();


//  Global Stats

void report_elapsed_stats(int depth);


// IO Event Tracking

/** IO Event type identifiers.
 *
 * Statistics for each event type are recorded separately.
 */
typedef enum {
   IE_WRITE,               ///< write event
   IE_READ,                ///< read event
   IE_WRITE_READ,          ///< write/read operation, typical for I2C
   IE_OPEN,                ///< device file open
   IE_CLOSE,               ///< device file close
   IE_OTHER                ///< other IO event
} IO_Event_Type;


const char * io_event_name(IO_Event_Type event_type);


void log_io_call(
        const IO_Event_Type  event_type,
        const char *         location,
        uint64_t             start_time_nanos,
        uint64_t             end_time_nanos);

#define RECORD_IO_EVENT(event_type, cmd_to_time)  { \
   long _start_time = cur_realtime_nanosec(); \
   cmd_to_time; \
   log_io_call(event_type, __func__, _start_time, cur_realtime_nanosec()); \
}

void report_io_call_stats(int depth);


// Record Status Code Occurrence

Public_Status_Code log_status_code(Public_Status_Code rc, const char * caller_name);
Public_Status_Code log_retryable_status_code(Public_Status_Code rc, const char * caller_name);
#define COUNT_STATUS_CODE(rc) log_status_code(rc,__func__)
#define COUNT_RETRYABLE_STATUS_CODE(rc) log_retryable_status_code(rc,__func__)
void show_all_status_counts();


// Sleep Strategy

bool   set_sleep_strategy(int strategy);
int    get_sleep_strategy();
char * sleep_strategy_desc(int sleep_strategy);

/** Sleep event type */
typedef enum {
   SE_WRITE_TO_READ,         ///< between I2C write and read
   SE_POST_OPEN,             ///< after I2C device opened
   SE_POST_WRITE,            ///< after I2C write without subsequent read
   SE_POST_READ,             ///< after I2C read
   SE_DDC_NULL,              ///< after DDC Null response
   SE_POST_SAVE_SETTINGS     ///< after DDC Save Current Settings command
} Sleep_Event_Type;
const char * sleep_event_name(Sleep_Event_Type event_type);



// Functions for sleeping.  The actual sleep time is determined
// by the strategy in place given the situation in which sleep is invoked.

// Convenience methods that call call_tuned_sleep():
void call_tuned_sleep_i2c(Sleep_Event_Type event_type);   // DDC_IO_DEVI2C
void call_tuned_sleep_adl(Sleep_Event_Type event_type);   // DDC_IO_ADL
void call_tuned_sleep_dh(Display_Handle* dh, Sleep_Event_Type event_type);
// The workhorse:
void call_tuned_sleep(DDCA_IO_Mode io_mode, Sleep_Event_Type event_type);
void call_dynamic_tuned_sleep( DDCA_IO_Mode io_mode,Sleep_Event_Type event_type, int occno);
void call_dynamic_tuned_sleep_i2c(Sleep_Event_Type event_type, int occno);

void report_sleep_strategy_stats(int depth);

#endif /* EXECUTION_STATS_H_ */
