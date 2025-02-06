/** @file timestamp.h
 *
 *  Timestamp management
 */

// Copyright (C) 2014-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TIMESTAMP_H_
#define TIMESTAMP_H_

#ifdef __cplusplus
extern "C" {
#endif

/** \cond */
#include <glib-2.0/glib.h>
/** \endcond */

//
// Timestamp Generation
//
uint64_t cur_realtime_nanosec();   // Returns the current value of the realtime clock in nanoseconds
void     show_timestamp_history(); // For debugging
uint64_t elapsed_time_nanosec();   // nanoseconds since start of program, first call initializes
char *   formatted_elapsed_time_t(guint precision); // printable elapsed time
char *   formatted_time_t(uint64_t nanos);
char *   formatted_epoch_time_t(time_t epoch_seconds);

#define NANOS2MICROS( _nanosec ) (((_nanosec) + 500)/1000)
#define NANOS2MILLIS( _nanosec ) (((_nanosec)+500000)/1000000)
#define MILLIS2NANOS( _millisec) (_millisec*(uint64_t)1000000)
#define MILLIS2MICROS(_millisec) (_millisec*(uint64_t)1000)

#ifdef __cplusplus
}
#endif

#endif /* TIMESTAMP_H_ */
