/* timestamp.h
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

/** \file 
 *Timestamp Generation
 */

#ifndef TIMESTAMP_H_
#define TIMESTAMP_H_

#include <stdint.h>

//
// Timestamp Generation
//
uint64_t cur_realtime_nanosec();   // Returns the current value of the realtime clock in nanoseconds
void     show_timestamp_history(); // For debugging
uint64_t elapsed_time_nanosec();   // nanoseconds since start of program, first call initializes
char *   formatted_elapsed_time(); // printable elapsed time

#endif /* TIMESTAMP_H_ */
