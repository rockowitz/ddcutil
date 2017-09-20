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

/** \cond */
#include <assert.h>
// #include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */



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
gint gaux_ptr_scomp(gconstpointer a, gconstpointer b) {
   char ** ap = (char **) a;
   char ** bp = (char **) b;
   // printf("(%s) ap = %p -> -> %p -> |%s|\n", __func__, ap, *ap, *ap);
   // printf("(%s) bp = %p -> -> %p -> |%s|\n", __func__, bp, *bp, *bp);
   return g_ascii_strcasecmp(*ap,*bp);
}


/** Formats a string similarly to g_sprintf(), but allocates
 *  a sufficiently sized buffer in which the formatted string
 *  is returned.
 *
 *  \param fmt  format string
 *  \param ...  arguments
 *  \return     pointer to newly allocated string
 */
gchar * gaux_asprintf(gchar * fmt, ...) {
   char * result = NULL;
   va_list(args);
   va_start(args, fmt);

   // g_vasprintf(&result, fmt, args);  // get implicit function declaration error

   gsize sz = g_printf_string_upper_bound(fmt,args);
   result = calloc(1,sz);
   va_start(args, fmt);
   g_vsnprintf(result, sz, fmt, args);
   va_end(args);
   return result;
}


//
// Thread utilities
//

/** Handles the boilerplate of obtaining a thread specific buffer that can
 *  change size.
 *
 *  If parm **bufsz_key_ptr** is NULL, the buffer is reallocated with the
 *  specified size with each call to this function.
 *
 *  If parm **bufsz_key_ptr** is non-NULL, then the buffer is reallocated
 *  only if the requested size is larger than the current size.  That is,
 *  the buffer can grow in size but never shrink.
 *
 *  \param  buf_key_ptr    address of a **GPrivate** used as the identifier
 *                         for the buffer
 *  \param  bufsz_key_ptr  address of **GPrivate** used as an identifier for
 *                         the current buffer size
 *  \param  required_size  size of buffer to allocate
 *  \return pointer to thread specific buffer
 */
gchar *
get_thread_dynamic_buffer(
      GPrivate * buf_key_ptr,
      GPrivate * bufsz_key_ptr,
      guint16    required_size)
{
   // printf("(%s) buf_key_ptr=%p, bufsz_key_ptr=%p, required_size=%d\n",
   //        __func__, buf_key_ptr, bufsz_key_ptr, required_size);

   char * buf       = g_private_get(buf_key_ptr);
   int * bufsz_ptr  = NULL;
   if (bufsz_key_ptr)
      bufsz_ptr = g_private_get(bufsz_key_ptr);

   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, buf=%p, bufsz_ptr=%p\n", __func__, this_thread, buf, bufsz_ptr);
   // if (bufsz_ptr)
   //    printf("(%s) *bufsz_ptr = %d\n", __func__, *bufsz_ptr);


   // unnecessary if use g_private_replace() instead of g_private_set()
   // if (buf)
   //    g_free(buf);

   if ( !bufsz_ptr || *bufsz_ptr < required_size) {
      buf = g_new(char, required_size);
      // printf("(%s) Calling g_private_set()\n", __func__);
      g_private_replace(buf_key_ptr, buf);

      if (bufsz_key_ptr) {
         if (!bufsz_ptr) {
            bufsz_ptr = g_new(int, 1);
            g_private_set(bufsz_key_ptr, bufsz_ptr);
         }
         *bufsz_ptr = required_size;
      }
   }

   // printf("(%s) Returning: %p\n", __func__, buf);
   return buf;
}


/** Handles the boilerplate of obtaining a thread specific fixed size buffer.
 *  The first call to this function in a thread with a given key address
 *  allocates the buffer.  Subsequent calls in the thread for the same key
 *  address return the same buffer.
 *
 *  \param  buf_key_ptr  address of a **GPrivate** used as the identifier
 *                       for the buffer
 *  \param  buffer_size  size of buffer to allocate
 *  \return pointer to thread specific buffer
 *
 *  \remark
 *  When the buffer is first allocated, byte 0 is set to '\0'
 */
gchar *
get_thread_fixed_buffer(
      GPrivate * buf_key_ptr,
      guint16    buffer_size)
{
   // printf("(%s) buf_key_ptr=%p, buffer_size=%d\n", __func__, buf_key_ptr, buffer_size);
   assert(buffer_size > 0);

   char * buf = g_private_get(buf_key_ptr);

   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, buf=%p\n", __func__, this_thread, buf);

   if (!buf) {
      buf = g_new(char, buffer_size);
      buf[0] = '\0';     // (sort of) mark buffer as unused
      g_private_set(buf_key_ptr, buf);
   }

   // printf("(%s) Returning: %p\n", __func__, buf);
   return buf;
}


