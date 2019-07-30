/** @file glib_string_util.c
 *
 *  Functions that depend on both glib_util.c and string_util.c.
 *
 *  glib_string_util.c/h exists to avoid circular dependencies within directory util.
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "string_util.h"

#include "glib_string_util.h"


/** Joins a GPtrArray containing pointers to character strings
 *  into a single string,
 *
 *  @param strings   GPtrArray of strings
 *  @param sepstr   if non-null, separator to insert between joined strings
 *
 *  @return joined string, caller is responsible for freeing
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

   free(pieces);
   return catenated;
}


/** Joins a GPtrArray containing pointers to character strings
 *  into a single string,
 *
 *  The result is returned in a thread-specific private buffer that is
 *  valid until the next call of this function in the current thread.
 *
 *  @param strings   GPtrArray of strings
 *  @param sepstr   if non-null, separator to insert between joined strings
 *
 *  @return joined string, do not free
 */

char * join_string_g_ptr_array_t(GPtrArray* strings, char * sepstr) {
   static GPrivate  buffer_key = G_PRIVATE_INIT(g_free);
   static GPrivate  buffer_len_key = G_PRIVATE_INIT(g_free);

   char * catenated = join_string_g_ptr_array(strings, sepstr);
   int required_size = strlen(catenated) + 1;
   char * buf = get_thread_dynamic_buffer(&buffer_key, &buffer_len_key, required_size);
   strncpy(buf, catenated, required_size);
   free(catenated);
   return buf;
}


/** Looks for a string in a **GPtrArray** of strings.
 *
 * @param haystack **GPtrArray** to search
 * @param needle   string to search for (case sensitive)
 * @return index of string if found, -1 if not found
 *
 * @remark
 * glib function **g_ptr_array_find_with_equal_funct()** is an obvious alternative,
 * but it requires glib version >= 2.54
 */
int gaux_string_ptr_array_find(GPtrArray * haystack, const char * needle) {
   int result = -1;
   for (int ndx = 0; ndx < haystack->len; ndx++) {
      if (streq(needle, g_ptr_array_index(haystack, ndx))) {
         result = ndx;
         break;
      }
   }
   return result;
}

