/** \file sleep.h
 * Sleep Management
 *
 * Sleeps are integral to the DDC protocol.  Most of **ddcutil's** elapsed
 * time is spent in sleeps mandated by the DDC protocol.
 * Sleep invocation is centralized here to keep statistics and facilitate
 * future tuning.
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef BASE_SLEEP_H_
#define BASE_SLEEP_H_

#include <inttypes.h>

//
// Sleep and sleep statistics
//

void sleep_millis(int milliseconds);
void sleep_millis_with_trace(
        int          milliseconds,
        const char * caller_location,
        const char * message);
void sleep_millis_with_tracex(
        int          milliseconds,
        const char * func,
        int          lineno,
        const char * filename,
        const char * message);

#define SLEEP_MILLIS_WITH_TRACE(_millis, _msg) \
   sleep_millis_with_tracex(_millis, __func__, __LINE__, __FILE__, _msg)

typedef struct {
   uint64_t actual_sleep_nanos;
   int      requested_sleep_milliseconds;
   int      total_sleep_calls;
} Sleep_Stats;

void         init_sleep_stats();
Sleep_Stats  get_sleep_stats();
void         report_sleep_stats(int depth);

#endif /* BASE_SLEEP_H_ */
