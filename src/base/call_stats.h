/*  call_stats.h
 *
 *  Created on: Oct 31, 2015
 *      Author: rock
 *
 *  For recording the count and elapsed time of system calls.
 */

#ifndef CALL_STATS_H_
#define CALL_STATS_H_

#include <stdbool.h>

#include <base/displays.h>
#include <base/status_code_mgt.h>


void init_call_stats();

typedef enum {IE_WRITE, IE_READ, IE_WRITE_READ, IE_OPEN, IE_CLOSE, IE_OTHER} IO_Event_Type;
typedef enum {SE_WRITE_TO_READ, SE_POST_OPEN, SE_POST_WRITE } Sleep_Event_Type;

const char * io_event_name(IO_Event_Type event_type);
const char * sleep_event_name(Sleep_Event_Type event_type);

// Records an IO function call
void log_io_event(
        const IO_Event_Type  event_type,
        const char *         location,
        long                 start_time_nanos,
        long                 end_time_nanos);
// Records when an IO function is found to have returned an error
void note_io_error(Global_Status_Code gsc, const char * location);

#define RECORD_IO_EVENT(event_type, cmd_to_time)  { \
   long _start_time = cur_realtime_nanosec(); \
   cmd_to_time; \
   log_io_event(event_type, __func__, _start_time, cur_realtime_nanosec()); \
}


// Functions for sleeping.  The actual sleep time is determined
// by the strategy in place given the situation in which sleep is invoked.

// Convenience methods that call sleep_io_mode():
void call_tuned_sleep_i2c(Sleep_Event_Type event_type);
void call_tuned_sleep_adl(Sleep_Event_Type event_type);
void call_tuned_sleep_dh(Display_Handle* dh, Sleep_Event_Type event_type);

// The workhorse
void call_tuned_sleep(DDC_IO_Mode io_mode, Sleep_Event_Type event_type);

// Reporting functions
void report_call_stats(int depth);
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
