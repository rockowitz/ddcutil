/* glib_util.h
 *
 * Created on: Jan 27, 2016
 *     Author: rock
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

#ifndef SRC_UTIL_GLIB_UTIL_H_
#define SRC_UTIL_GLIB_UTIL_H_

#include <glib.h>

#include "util/string_util.h"

GPtrArray * ntsa_to_g_ptr_array(Null_Terminated_String_Array ntsa);

Null_Terminated_String_Array g_ptr_array_to_ntsa(GPtrArray * garray);

#endif /* SRC_UTIL_GLIB_UTIL_H_ */
