/* glib_util.c
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

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "util/glib_util.h"


/* Converts a Null_Terminated_String_Array to a GPtrArry.
 * The underlying strings are referenced, not duplicated.
 */
GPtrArray * ntsa_to_g_ptr_array(Null_Terminated_String_Array ntsa) {
   int len = null_terminated_string_array_length(ntsa);
   GPtrArray * garray = g_ptr_array_sized_new(len);
   int ndx;
   for (ndx=0; ndx<len; ndx++) {
      g_ptr_array_add(garray, ntsa[ndx]);
   }
   return garray;
}


/* Converts a GPtrArray to a Null_Terminated_String_Array.
 * The underlying strings are referenced, not duplicated.
 */
Null_Terminated_String_Array g_ptr_array_to_ntsa(GPtrArray * garray) {
   assert(garray);
   Null_Terminated_String_Array ntsa = calloc(garray->len+1, sizeof(char *));
   int ndx = 0;
   for (;ndx < garray->len; ndx++) {
      ntsa[ndx] = g_ptr_array_index(garray,ndx);
   }
   return ntsa;
}
