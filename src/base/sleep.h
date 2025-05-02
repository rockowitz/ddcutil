/** \file sleep.h
 * Basic Sleep Services
 *
 * Most of **ddcutil's** elapsed time is spent in sleeps mandated by the
 * DDC protocol. Basic sleep invocation is centralized here to perform sleep
 * tracing and and maintain sleep statistics.
 */

// Copyright (C) 2014-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef BASE_SLEEP_H_
#define BASE_SLEEP_H_

#include <inttypes.h>

// Sleep statistics

typedef struct {
   uint64_t actual_sleep_nanos;
   int      requested_sleep_milliseconds;
   int      total_sleep_calls;
} Sleep_Stats;

void         init_sleep_stats();
Sleep_Stats  get_sleep_stats();
void         report_sleep_stats(int depth);


// Perform sleep

#ifdef OLD
void sleep_millis(int milliseconds);
#endif

#ifdef OLD
void sleep_millis_with_trace(
        int          milliseconds,
        const char * func,
        int          lineno,
        const char * filename,
        const char * message);

#define SLEEP_MILLIS_TRACEABLE(_millis, _msg) \
   sleep_millis_with_trace(_millis, __func__, __LINE__, __FILE__, _msg)
#endif

#ifdef OLD
#define SLEEP_MILLIS_WITH_SYSLOG(_millis, _msg) \
   do { \
      sleep_millis_with_syslog(DDCA_SYSLOG_NOTICE, __func__, __LINE__, __FILE__, _millis, _msg); \
   } while(0)
#define SLEEP_MILLIS_WITH_SYSLOG2(_syslog_level, _millis, _msg) \
   do { \
      sleep_millis_with_syslog(_syslog_level, __func__, __LINE__, __FILE__, _millis, _msg); \
   } while(0)
void sleep_millis_with_syslog(DDCA_Syslog_Level level, const char * func, int line, const char * file, uint millis, const char * msg);
#endif

#ifdef OLD
void sleep_millis_with_syslog(DDCA_Syslog_Level level, const char * func, int line, const char * file, uint millis, const char * msg);
#endif


typedef enum {
   SLEEP_OPT_NONE      = 0,
   SLEEP_OPT_TRACEABLE = 1,
   SLEEP_OPT_STATS     = 2,
} Loggable_Sleep_Options;


/** General function for performing sleep.
 *
 *  This is a merger of all the sleep function variants previously defined
 *  in sleep.c
 *
 * Sleep function variants are implemented as macros.
 */
void loggable_sleep(
      int                    millisec,
      Loggable_Sleep_Options opts,
      DDCA_Syslog_Level      syslog_level,
      const char *           func,
      int                    lineno,
      const char *           filename,
      const char *           format, ...);


#define LOGGABLE_SLEEP(_millis, _opts, _syslog_level, _msg, ...) \
   do { \
      loggable_sleep(_millis, _opts, _syslog_level, \
                     __func__, __LINE__, __FILE__, _msg,  ##__VA_ARGS__); \
   } while(0)


/** Sleep for the specified number of milliseconds.
 *
 * \param milliseconds number of milliseconds to sleep
 */
#define SLEEP_MILLIS_SIMPLE(_millis) \
   do { \
      loggable_sleep(_millis, SLEEP_OPT_NONE, DDCA_SYSLOG_NEVER, __func__, __LINE__, __FILE__, NULL); \
   } while(0)


/** Sleep for the specified number of milliseconds and
 *  record sleep statistics.
 *
 * \param milliseconds number of milliseconds to sleep
 */
#define SLEEP_MILLIS_WITH_STATS(_millis) \
   do { \
      loggable_sleep(_millis, SLEEP_OPT_STATS, DDCA_SYSLOG_NEVER, __func__, __LINE__, __FILE__, NULL); \
   } while(0)


/** Sleep for the specified number of milliseconds, record
 *  sleep statistics, and optionally perform tracing.
 *
 * \param milliseconds number of milliseconds to sleep
 * \param msg
 */
#define SLEEP_MILLIS_TRACEABLE(_millis, _msg, ...) \
   do { \
      loggable_sleep(_millis, SLEEP_OPT_TRACEABLE, DDCA_SYSLOG_NEVER, \
                     __func__, __LINE__, __FILE__, _msg,  ##__VA_ARGS__); \
   } while(0)


#define SLEEP_MILLIS_WITH_SYSLOG(_millis, _msg, ...) \
   do { \
      loggable_sleep(_millis, SLEEP_OPT_NONE, DDCA_SYSLOG_NOTICE, \
                     __func__, __LINE__, __FILE__, _msg, ##__VA_ARGS__); \
   } while(0)

#define SLEEP_MILLIS_WITH_SYSLOG2(_syslog_level, _millis, _msg, ...) \
   do { \
      loggable_sleep(_millis, SLEEP_OPT_NONE, _syslog_level, \
                     __func__, __LINE__, __FILE__, _msg,  ##__VA_ARGS__); \
   } while(0)


#endif /* BASE_SLEEP_H_ */
