/* ddcg_cont_response.h
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

#ifndef DDCG_CONT_RESPONSE_H_
#define DDCG_CONT_RESPONSE_H_

#include <glib-object.h>

#include "public/ddcutil_c_api.h"

G_BEGIN_DECLS

#define DDCG_TYPE_CONT_RESPONSE (ddcg_cont_response_get_type())
G_DECLARE_FINAL_TYPE(DdcgContResponse, ddcg_cont_response, DDCG, CONT_RESPONSE, GObject)

#define DDCG_CONT_RESPONSE_ERROR       ( ddcg_cont_response_quark()    )
#define DDCG_CONT_RESPONSE_TYPE_ERROR  ( ddcg_cont_response_get_type() )


struct _DdcgContResponse {
   GObject   parent_instance;

   // class instance variables:
   DDCA_Non_Table_Value *  presp;

   guint8   mh;
   guint8   ml;
   guint8   sh;
   guint8   sl;
   gint32   max_value;       // or guint16?
   gint32   cur_value;
};

void ddcg_cont_response_report(DdcgContResponse * presp, int depth);

DdcgContResponse * ddcg_cont_response_new(void);

DdcgContResponse *
ddcg_cont_response_create(
      guint8    mh,
      guint8    ml,
      guint8    sh,
      guint8    sl);

G_END_DECLS

#endif /* _DDCG_CONT_RESPONSE_H_ */
