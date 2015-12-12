/* debug_util.h
 *
 * Created on: Nov 5, 2015
 *     Author: rock
 *
 * Generic debugging utilities
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef UTIL_DEBUG_UTIL_H_
#define UTIL_DEBUG_UTIL_H_

#include <stdlib.h>

//
// Traceable memory management
//

void * call_malloc(size_t size, char * loc);
void * call_calloc(size_t nelem, size_t elsize, char * loc);
void   call_free(void * ptr, char * loc);

#endif /* UTIL_DEBUG_UTIL_H_ */
