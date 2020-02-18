/** \file execution_stats.h
 *
 * Record the count and elapsed time of system calls.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


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
   uint64_t _start_time = cur_realtime_nanosec(); \
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




/** Sleep event type */
typedef enum {
   SE_WRITE_TO_READ,         ///< between I2C write and read
   SE_POST_OPEN,             ///< after I2C device opened
   SE_POST_WRITE,            ///< after I2C write without subsequent read
   SE_POST_READ,             ///< after I2C read
   SE_DDC_NULL,              ///< after DDC Null response
   SE_POST_SAVE_SETTINGS,    ///< after DDC Save Current Settings command
   SE_PRE_EDID,              ///< before reading EDID
   SE_OTHER,
   SE_SPECIAL                ///< explicit time specified
} Sleep_Event_Type;
const char * sleep_event_name(Sleep_Event_Type event_type);

void reset_sleep_event_counts();
void record_sleep_event(Sleep_Event_Type event_type);

void report_execution_stats(int depth);



#endif /* EXECUTION_STATS_H_ */
