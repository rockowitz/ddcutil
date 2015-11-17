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

//
// General timing statistics
//

typedef struct {
   long   total_call_nanosecs;
   int    total_call_ct;
   char*  stat_name;       // is this useful?
} Call_Stats;

typedef struct {
   Call_Stats * pread_write_stats;
   Call_Stats * popen_stats;
   Call_Stats * pclose_stats;
   bool         stats_active;
} I2C_Call_Stats;

typedef struct {
   Call_Stats * pread_write_stats;
   Call_Stats * pother_stats;
   bool         stats_active;
} ADL_Call_Stats;


#define RECORD_TIMING_STATS(pstats, cmd_to_time)  { \
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
}


// Similar to RECORD_TIMING_STATS, but do not save and preserve errno
#define RECORD_TIMING_STATS_NOERRNO(pstats, cmd_to_time)  { \
   long _start_time = 0; \
   if (pstats) {\
     _start_time = cur_realtime_nanosec(); \
   } \
   cmd_to_time; \
   if (pstats) { \
      pstats->total_call_nanosecs += (cur_realtime_nanosec()-_start_time); \
      pstats->total_call_ct++; \
   } \
}


void report_one_call_stat(Call_Stats * pstats, int depth);

I2C_Call_Stats * new_i2c_call_stats();
ADL_Call_Stats * new_adl_call_stats();

void report_i2c_call_stats(I2C_Call_Stats * pi2c_call_stats, int depth);
void report_adl_call_stats(ADL_Call_Stats * padl_call_stats, int depth);

#endif /* CALL_STATS_H_ */
