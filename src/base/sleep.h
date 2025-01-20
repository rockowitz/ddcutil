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

// Perform sleep

void sleep_millis(int milliseconds);
void sleep_millis_with_trace(
        int          milliseconds,
        const char * func,
        int          lineno,
        const char * filename,
        const char * message);

#define SLEEP_MILLIS_WITH_TRACE(_millis, _msg) \
   sleep_millis_with_trace(_millis, __func__, __LINE__, __FILE__, _msg)

// Sleep statistics

typedef struct {
   uint64_t actual_sleep_nanos;
   int      requested_sleep_milliseconds;
   int      total_sleep_calls;
} Sleep_Stats;

void         init_sleep_stats();
Sleep_Stats  get_sleep_stats();
void         report_sleep_stats(int depth);

// Perform display watch related sleeps

#define DW_SLEEP_MILLIS(_millis, _msg) \
   do { \
      dw_sleep_millis(DDCA_SYSLOG_NOTICE, __func__, __LINE__, __FILE__, _millis, _msg); \
   } while(0)
#define DW_SLEEP_MILLIS2(_syslog_level, _millis, _msg) \
   do { \
      dw_sleep_millis(_syslog_level, __func__, __LINE__, __FILE__, _millis, _msg); \
   } while(0)
void dw_sleep_millis(DDCA_Syslog_Level level, const char * func, int line, const char * file, uint millis, const char * msg);

#endif /* BASE_SLEEP_H_ */
