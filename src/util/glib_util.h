/* glib_util.h
 *
 * Utility functions for glib.
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

/** @file glib_util.h
 *  Functions for use with glib
 */

#ifndef GLIB_UTIL_H_
#define GLIB_UTIL_H_

/** \cond */
#include <glib-2.0/glib.h>
/** \endcond */

gpointer * g_list_to_g_array(GList * glist, guint * length);

gint gaux_ptr_scomp(gconstpointer a, gconstpointer b);

gchar * gaux_asprintf(gchar * fmt, ...);

gchar *
get_thread_dynamic_buffer(
      GPrivate * buf_key_ptr,
      GPrivate * bufsz_key_ptr,
      guint16    required_size);

gchar *
get_thread_fixed_buffer(
      GPrivate * buf_key_ptr,
      guint16    required_size);

GPtrArray * gaux_ptr_array_truncate(GPtrArray * gpa, int limit);

// Future:
typedef  gpointer (*GAuxDupFunc)(gpointer src);

GPtrArray *
gaux_ptr_array_append_array(
      GPtrArray * dest,
      GPtrArray * src,
      GAuxDupFunc dup_func);

GPtrArray *
gaux_ptr_array_join(
      GPtrArray *    gpa1,
      GPtrArray *    gpa2,
      GAuxDupFunc    dup_func,
      GDestroyNotify element_free_func);

GPtrArray *
gaux_ptr_array_copy(
      GPtrArray *    src,
      GAuxDupFunc    dup_func,
      GDestroyNotify element_free_func);

GPtrArray *
gaux_ptr_array_from_null_terminated_array(
      gpointer *     src,
      GAuxDupFunc    dup_func,
      GDestroyNotify element_free_func);



#endif /* GLIB_UTIL_H_ */
