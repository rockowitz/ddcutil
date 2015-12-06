/*  execution_stats.h
 *
 *  Created on: Oct 31, 2015
 *      Author: rock
 *
 *  For recording the count and elapsed time of system calls.
 */

#ifndef CALL_STATS_H_
#define CALL_STATS_H_

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

typedef enum {SE_WRITE_TO_READ, SE_POST_OPEN, SE_POST_WRITE, SE_POST_READ } Sleep_Event_Type;
const char * sleep_event_name(Sleep_Event_Type event_type);

// Functions for sleeping.  The actual sleep time is determined
// by the strategy in place given the situation in which sleep is invoked.

// Convenience methods that call call_tuned_sleep():
void call_tuned_sleep_i2c(Sleep_Event_Type event_type);   // DDC_IO_DEVI2C
void call_tuned_sleep_adl(Sleep_Event_Type event_type);   // DDC_IO_ADL
void call_tuned_sleep_dh(Display_Handle* dh, Sleep_Event_Type event_type);
// The workhorse:
void call_tuned_sleep(DDC_IO_Mode io_mode, Sleep_Event_Type event_type);

void report_sleep_strategy_stats(int depth);




#ifdef OLD
#define RECORD_TIMING_STATS(pstats, event_type, cmd_to_time)  { \
   int  _errsv = 0; \
   long _start_time = 0; \
   if (pstats) {\
     _start_time = cur_realtime_nanosec(); \
   } \
   cmd_to_time; \
   if (pstats) { \
      _errsv = errno; \
      pstats->total_call_nanosecs += (cur_realtime_nanosec()-_start_time); \
      pstats->total_call_ct++; \
      errno = _errsv; \
   } \
   note_io_event(event_type,__func__); \
}
#endif


#ifdef OLD
// Similar to RECORD_TIMING_STATS, but do not save and preserve errno
#define RECORD_TIMING_STATS_NOERRNO(pstats, event_type, cmd_to_time)  { \
   long _start_time = 0; \
   if (pstats) {\
     _start_time = cur_realtime_nanosec(); \
   } \
   cmd_to_time; \
   if (pstats) { \
      pstats->total_call_nanosecs += (cur_realtime_nanosec()-_start_time); \
      pstats->total_call_ct++; \
   } \
   note_io_event(event_type,__func__); \
}
#endif

#endif /* CALL_STATS_H_ */
