/** @file glib_string_util.h
 *
 *  Functions that depend on both glib_util.c and string_util.c
 *
 *  glib_string_util.c/h exist to avoid circular dependencies.
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


/** \cond */
#include <glib-2.0/glib.h>
/** \endcond */

#ifndef GLIB_STRING_UTIL_H_
#define GLIB_STRING_UTIL_H_

char * join_string_g_ptr_array(GPtrArray* strings, char * sepstr);
char * join_string_g_ptr_array_t(GPtrArray* strings, char * sepstr);

int gaux_string_ptr_array_find(GPtrArray * haystack, const char * needle);

#endif /* GLIB_STRING_UTIL_H_ */
