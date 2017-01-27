/* glib_util.c
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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
// #include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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


/** Joins a GPtrArray containing pointers to character strings
 *  into a single string,
 *
 *  Arguments:
 *     string   GPtrArray of strings
 *     sepstr   if non-null, separator to insert between joined strings
 *
 *  Returns:
 *     joined string
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

/**
 * g_hash_table_get_keys_as_array:
 * @hash_table: a #GHashTable
 * @length: (out): the length of the returned array
 *
 * Retrieves every key inside @hash_table, as an array.
 *
 * The returned array is %NULL-terminated but may contain %NULL as a
 * key.  Use @length to determine the true length if it's possible that
 * %NULL was used as the value for a key.
 *
 * Note: in the common case of a string-keyed #GHashTable, the return
 * value of this function can be conveniently cast to (gchar **).
 *
 * You should always free the return result with g_free().  In the
 * above-mentioned case of a string-keyed hash table, it may be
 * appropriate to use g_strfreev() if you call g_hash_table_steal_all()
 * first to transfer ownership of the keys.
 *
 * Returns: (array length=length) (transfer container): a
 *   %NULL-terminated array containing each key from the table.
 *
 * Since: 2.40
 **/
/*
gpointer *
g_hash_table_get_keys_as_array_local (GHashTable *hash_table,
                                      guint      *length)
{
  gpointer *result;
  guint i, j = 0;

  result = g_new(gpointer, hash_table->nnodes + 1);
  for (i = 0; i < hash_table->size; i++)
    {
      if (HASH_IS_REAL (hash_table->hashes[i]))
        result[j++] = hash_table->keys[i];
    }
  g_assert_cmpint (j, ==, hash_table->nnodes);
  result[j] = NULL;

  if (length)
    *length = j;

  return result;
}
*/

gpointer * g_list_to_g_array(GList * glist, guint * length) {
   int len = 0;
   gpointer * result = NULL;
   guint ndx = 0;
   GList * lptr;

   len = g_list_length(glist);
   result = g_new(gpointer, len+1);
   for (lptr = glist; lptr!=NULL; lptr=lptr->next) {
      result[ndx++] = lptr->data;
   }
   result[ndx] = NULL;

   *length = len;
   return result;
}
