/** @file glib_util.h
 *
 *  Utility functions for glib.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GLIB_UTIL_H_
#define GLIB_UTIL_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdio.h>
/** \endcond */

gpointer * g_list_to_g_array(GList * glist, guint * length);

gint gaux_ptr_scomp(gconstpointer a, gconstpointer b);

#ifdef OLD
gchar * gaux_vasprintf(size_t reqd_buf_size, gchar * fmt, va_list ap);
gchar * gaux_asprintf(gchar * fmt, ...);
#endif

void *
get_thread_dynamic_buffer(
      GPrivate * buf_key_ptr,
      GPrivate * bufsz_key_ptr,
      guint16    required_size);

void *
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
