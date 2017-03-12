/* glib_util.c
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

/** @file glib_util.c
 *
 *  Functions for use with glib.
 */

#include <assert.h>
// #include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glib_util.h"

#ifdef ALTERNATIVE

// create private copy of g_hash_table_get_keys_as_array()

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
#endif


/** Converts a doubly linked list of pointers into a null-terminated array
 * of pointers.
 *
 * @param  glist     pointer to doubly linked list
 * @param  length    where to return number of items in the allocated array,
 *                   (not including the final NULL terminating entry
 *
 * @return pointer to the newly allocated gpointer array
 *
 * @remark The pointers in the linked list are copied to the newly allocated array.
 *   The data pointed to is not duplicated.
 * @remark This function is needed because glib function g_hash_table_get_keys_as_array()
 *   does not exist in glib versions less than 2.40
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


/** String comparison function used by g_ptr_array_sort()
 *
 * @param a pointer to first string
 * @param b pointer to second string
 * @return -1, 0, +1 in the usual way
 */
gint g_ptr_scomp(gconstpointer a, gconstpointer b) {
   char ** ap = (char **) a;
   char ** bp = (char **) b;
   // printf("(%s) ap = %p -> -> %p -> |%s|\n", __func__, ap, *ap, *ap);
   // printf("(%s) bp = %p -> -> %p -> |%s|\n", __func__, bp, *bp, *bp);
   return g_ascii_strcasecmp(*ap,*bp);
}
