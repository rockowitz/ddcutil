/* sleep.h
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

/** \file sleep.h
 * Sleep Management
 *
 * Sleeps are integral to the DDC protocol.  Most of **ddcutil's** elapsed
 * time is spent in sleeps mandated by the DDC protocol.
 * Sleep invocation is centralized here to keep statistics and facilitate
 * future tuning.
 */

#ifndef BASE_SLEEP_H_
#define BASE_SLEEP_H_

//
// Sleep and sleep statistics
//

void sleep_millis( int milliseconds);
void sleep_millis_with_trace(int milliseconds, const char * caller_location, const char * message);

typedef struct {
   long requested_sleep_milliseconds;
   long actual_sleep_nanos;
   int  total_sleep_calls;
} Sleep_Stats;

void init_sleep_stats();
Sleep_Stats * get_sleep_stats();
void report_sleep_stats(int depth);

#endif /* BASE_SLEEP_H_ */
