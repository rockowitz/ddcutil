/** @file timestamp.h
 *
 *  Timestamp management
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TIMESTAMP_H_
#define TIMESTAMP_H_

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
char *   formatted_epoch_time_t(long epoch_seconds);

#endif /* TIMESTAMP_H_ */
