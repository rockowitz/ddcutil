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

#include <base/common.h>   // for sleep functions
#include <base/displays.h>
#include <base/status_code_mgt.h>


//
// General timing statistics
//

typedef struct {
   long   total_call_nanosecs;
   int    total_call_ct;
   char*  stat_name;       // is this useful?
} Single_Call_Stat;

typedef struct {
   Single_Call_Stat * pread_write_stats;
   Single_Call_Stat * popen_stats;
   Single_Call_Stat * pclose_stats;
   Single_Call_Stat * pother_stats;
   bool               stats_active;
} DDC_Call_Stats;

#ifdef OLD
typedef struct {
   Single_Call_Stat * pread_write_stats;
   Single_Call_Stat * popen_stats;
   Single_Call_Stat * pclose_stats;
   bool         stats_active;
} I2C_Call_Stats;

typedef struct {
   Single_Call_Stat * pread_write_stats;
   Single_Call_Stat * pother_stats;
   bool         stats_active;
} ADL_Call_Stats;
#endif


typedef enum {IE_WRITE, IE_READ, IE_WRITE_READ, IE_OPEN, IE_CLOSE, IE_OTHER} IO_Event_Type;
typedef enum {SE_WRITE_TO_READ, SE_POST_OPEN, SE_POST_WRITE } Sleep_Event_Type;

const char * io_event_name(IO_Event_Type event_type);
const char * sleep_event_name(Sleep_Event_Type event_type);

void note_io_event(IO_Event_Type event_type, const char * location);
void note_io_event_time(IO_Event_Type event_type, const char * location, long when_nanos);
void note_io_error(Global_Status_Code gsc, const char * location);
void report_sleep_strategy_stats(int depth);



void record_io_event(const IO_Event_Type event_type, const char * location, long start_time_nanos, long end_time_nanos);


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

#define RECORD_IO_EVENT(event_type, cmd_to_time)  { \
   long _start_time = cur_realtime_nanosec(); \
   cmd_to_time; \
   record_io_event(event_type, __func__, _start_time, cur_realtime_nanosec()); \
}


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


void report_one_call_stat(Single_Call_Stat * pstats, int depth);

#ifdef OLD
I2C_Call_Stats * new_i2c_call_stats();
ADL_Call_Stats * new_adl_call_stats();


void report_i2c_call_stats(I2C_Call_Stats * pi2c_call_stats, int depth);
void report_adl_call_stats(ADL_Call_Stats * padl_call_stats, int depth);
#endif

void report_call_stats(int depth);

void init_call_stats();





// Convenience methods that call sleep_io_mode():
void sleep_i2c(Sleep_Event_Type event_type);
void sleep_adl(Sleep_Event_Type event_type);
void sleep_dh(Display_Handle* dh, Sleep_Event_Type event_type);

// The workhorse
void sleep_io_mode(DDC_IO_Mode io_mode, Sleep_Event_Type event_type);





#endif /* CALL_STATS_H_ */
