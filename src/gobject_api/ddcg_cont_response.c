/* ddcg_cont_response.c
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

#include "util/report_util.h"

#include "base/core.h"

#include "gobject_api/ddcg_cont_response.h"
#include "../libmain/ddcutil_c_api.h"


typedef struct {
   // whatever
} DdcgContResponsePrivate;


/**
 * ddcg_cont_response_report:
 * @presp:
 * @depth:  logical indentation depth
 *
 * Returns: nothing
 */
void ddcg_cont_response_report(DdcgContResponse * presp, int depth) {
   // g_return_if_fail( DDCG_IS_CONT_RESPONSE(presp) );   // flags failure, why?
   int d1 = depth+1;
   //int d2 = depth+2;
   rpt_vstring(depth, "DdcgContResponse at %p", presp);
   rpt_vstring(d1, "mh: 0x%02x", presp->mh);
   rpt_vstring(d1, "ml: 0x%02x", presp->ml);
   rpt_vstring(d1, "sh: 0x%02x", presp->sh);
   rpt_vstring(d1, "sl: 0x%02x", presp->sl);
   rpt_vstring(d1, "cur_value:  %d", presp->cur_value);
   rpt_vstring(d1, "max_value:  %d", presp->max_value);
   // rpt_vstring(d1, "pcontext:        %p", presp->pcontext);
   // rpt_vstring(d1, "priv:            %p", presp->priv);


}



G_DEFINE_TYPE (DdcgContResponse, ddcg_cont_response, G_TYPE_OBJECT);


static void ddcg_cont_response_class_init(DdcgContResponseClass * cls);
static void ddcg_cont_response_init(DdcgContResponse * cont_response);
#ifdef UNUSED
static void ddcg_cont_response_finalize(GObject * obj);
#endif

#ifdef NO
static void ddcg_cont_response_constructed(GObject * obj) {
   DBGMSG("Starting");
   // Update the object state depending on constructor properties

   // Chain up to parent constructed function to complete object initialization
   //G_OBJECT_CLASS(obj)->parent_class->constructed(obj);

}
#endif




static void ddcg_cont_response_class_init(DdcgContResponseClass * cls) {
   DBGMSG("Starting");
#ifdef WONT_COMPILE
   GObjectClass * object_class = G_OBJECT_CLASS(cls);
   object_class->constructed = ddcg_cont_response_constructed;
#endif
   ddca_init();
   // cls->class_initialized = true;  // no member named class_initialized

}


static void ddcg_cont_response_init(DdcgContResponse * cont_response) {
   DBGMSG("Starting");
   // initialize the instance

}

#ifdef UNUSED
static void ddcg_cont_response_finalize(GObject * obj) {
}
#endif

DdcgContResponse * ddcg_cont_response_new(void) {
   return g_object_new(DDCG_TYPE_CONT_RESPONSE, NULL);
}



/**
 * ddcg_cont_response_create: (constructor):
 * @mh:
 * @ml:
 * @sh:
 * @sl:
 *
 * Retrieve a raw non-table VCP feature value
 *
 * Returns:  (transfer full): point to #DdcgContResponse
 */
DdcgContResponse *
ddcg_cont_response_create(
      guint8    mh,
      guint8    ml,
      guint8    sh,
      guint8    sl)
{
   DdcgContResponse * ddcg_response = NULL;

   // allocate a new DdcgContResponse instance
      ddcg_response = g_object_new(DDCG_TYPE_CONT_RESPONSE, NULL);

      // or set properties?
      ddcg_response->mh = mh;
      ddcg_response->ml = ml;
      ddcg_response->sh = sh;
      ddcg_response->sl = sl;
      ddcg_response->cur_value = sh << 8 | sl;
      ddcg_response->max_value = mh << 8 | ml;
      ddcg_cont_response_report(ddcg_response, 1);


   DBGMSG("Returning ddcg_response=%p", ddcg_response);
   return ddcg_response;
}



