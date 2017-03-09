/* glib_string_util.c
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

/** @f glib_string_util.c
 * Functions that depend on both glib_util.c and string_util.c.
 *
 * glib_string_util.c/h exists to avoid circular dependencies within
 * directory util.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string_util.h"

#include "glib_string_util.h"


/** Joins a GPtrArray containing pointers to character strings
 *  into a single string,
 *
 *  @param strings   GPtrArray of strings
 *  @param sepstr   if non-null, separator to insert between joined strings
 *
 *  @return joined string
 */
char * join_string_g_ptr_array(GPtrArray* strings, char * sepstr) {
   bool debug = false;

   int ct = strings->len;
   if (debug)
      fprintf(stdout, "(%s) ct = %d\n", __func__, ct);
   char ** pieces = calloc(ct, sizeof(char*));
   int ndx;
   for (ndx=0; ndx < ct; ndx++) {
      pieces[ndx] = g_ptr_array_index(strings,ndx);
      if (debug)
         fprintf(stdout, "(%s) pieces[%d] = %s\n", __func__, ndx, pieces[ndx]);
   }
   char * catenated = strjoin((const char**) pieces, ct, sepstr);
   if (debug)
      fprintf(stdout, "(%s) strlen(catenated)=%zd, catenated=%p, catenated=|%s|\n",
                      __func__, strlen(catenated), catenated, catenated);

#ifdef GLIB_VARIANT
   // GLIB variant failing when used with file.  why?
   Null_Terminated_String_Array ntsa_pieces = g_ptr_array_to_ntsa(strings);
   if (debug) {
      DBGMSG("ntsa_pieces before call to g_strjoinv():");
      null_terminated_string_array_show(ntsa_pieces);
   }
   // n. our Null_Terminated_String_Array is identical to glib's GStrv
   gchar sepchar = ';';
   gchar * catenated2 = g_strjoinv(&sepchar, ntsa_pieces);
   DBGMSF(debug, "catenated2=%p", catenated2);
   *pstring = catenated2;
   assert(strcmp(catenated, catenated2) == 0);
#endif

   return catenated;
}
