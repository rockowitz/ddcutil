/** @file timestamp.h
 *
 *  Timestamp management
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TIMESTAMP_H_
#define TIMESTAMP_H_

/** \cond */
#include <stdint.h>
/** \endcond */

//
// Timestamp Generation
//
uint64_t cur_realtime_nanosec();   // Returns the current value of the realtime clock in nanoseconds
void     show_timestamp_history(); // For debugging
uint64_t elapsed_time_nanosec();   // nanoseconds since start of program, first call initializes
char *   formatted_elapsed_time(); // printable elapsed time

#endif /* TIMESTAMP_H_ */
