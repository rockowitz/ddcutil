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
#include <glib.h>
/** \endcond */

gpointer * g_list_to_g_array(GList * glist, guint * length);

gint g_ptr_scomp(gconstpointer a, gconstpointer b);

#endif /* GLIB_UTIL_H_ */
