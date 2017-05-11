/* ddg_display_handle.h
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

#ifndef _DDCG_DISPLAY_HANDLE_H_
#define _DDCG_DISPLAY_HANDLE_H_


#include <glib-object.h>
// #include <glib-2.0/glib-object.h>   // make eclipse happy

#include "gobject_api/ddcg_types.h"

G_BEGIN_DECLS

#define DDCG_TYPE_DISPLAY_HANDLE (ddcg_display_handle_get_type())
G_DECLARE_FINAL_TYPE(DdcgDisplayHandle, ddcg_display_handle, DDCG, DISPLAY_HANDLE, GObject)

#define DDCG_DISPLAY_HANDLE_ERROR       ( ddcg_display_handle_quark()    )
#define DDCG_DISPLAY_HANDLE_TYPE_ERROR  ( ddcg_display_handle_get_type() )

DdcgDisplayHandle *
ddcg_display_handle_new(void);

DdcgStatusCode
ddcg_display_handle_open0(
      DdcgDisplayRef *     ddcg_dref,
      DdcgDisplayHandle ** pddcg_dh);

DdcgDisplayHandle *
ddcg_display_handle_open(
      DdcgDisplayRef *     ddcg_dref,
      GError **            error);

DdcgStatusCode
ddcg_display_handle_close(DdcgDisplayHandle * ddcg_dh);


DdcgContResponse *
ddcg_display_handle_get_nontable_vcp_value(
      DdcgDisplayHandle *  ddcg_dh,
      DdcgFeatureCode      feature_code,
      GError **            error);

gchar *
ddcg_display_handle_repr(
      DdcgDisplayHandle *  ddcg_dh,
      GError **            error);

G_END_DECLS

#endif /* _DDCG_DISPLAY_HANDLE_H_ */
