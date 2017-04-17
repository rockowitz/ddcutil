/* ddcg_context.c
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

#include "base/core.h"

#include "gobject_api/ddcg_context.h"
#include "public/ddcutil_c_api.h"



#ifdef DOESNT_WORK
struct _DdcgContextClass {
   GObjectClass parent_class;
   bool     class_initialized;
};
#endif

struct _DdcgContext {
   GObject   parent_instance;

};


G_DEFINE_TYPE(DdcgContext, ddcg_context, G_TYPE_OBJECT)

static void ddcg_context_class_init(DdcgContextClass * cls);
static void ddcg_context_init(DdcgContext * context);
#ifdef UNUSED
static void ddcg_context_finalize(GObject * obj);
#endif

#ifdef NO
static void ddcg_context_constructed(GObject * obj) {
   DBGMSG("Starting");
   // Update the object state depending on constructor properties

   // Chain up to parent constructed function to complete object initialization
   G_OBJECT_CLASS(ddcg_context)->parent_class->constructed(obj));

}
#endif


static void ddcg_context_class_init(DdcgContextClass * cls) {
   DBGMSG("Starting");
#ifdef WONT_COMPILE
   GObjectClass * object_class = G_OBJECT_CLASS(cls);
   object_class->constructed = ddcg_context_constructed;
#endif
   // ddca_init();
   // cls->class_initialized = true;  // no member named class_initialized

}


static void ddcg_context_init(DdcgContext * context) {
   DBGMSG("Starting");
   // initialize the instance
}


#ifdef UNUSED
static void ddcg_context_finalize(GObject * obj) {
}
#endif

DdcgContext * ddcg_context_new(void) {
   return g_object_new(DDCG_TYPE_CONTEXT, NULL);
}


/**
 * ddcg_context_create_display_ref:
 * @ddcg_did: display identifier
 * @error: (out): location where to return error
 *
 * Creates a #DdcgDisplayRef from a #DdcgDisplayIdentifier.
 *
 * This may be a direct conversion (for busno or adlno) or may
 * entail searching the list of monitors detected.  If the
 * DisplayIdentifier does not refer to a valid monitor, an
 * error is returned.
 *
 * Returns: (transfer full): new #DdcgDisplayRef
 */
DdcgDisplayRef *
ddcg_context_create_display_ref(
      DdcgDisplayIdentifier * ddcg_did,
      GError **               error)
{
   g_return_val_if_fail (error == NULL || *error == NULL, NULL);

   DDCA_Display_Identifier ddct_did = NULL;   // extract from DdcgDisplayIdentifier
   ddct_did = _ddcg_display_identifier_get_ddct_object(ddcg_did);
   DdcgDisplayRef * ddcg_dref = NULL;
   DDCA_Display_Ref ddct_dref = NULL;     // is pointer
   DDCA_Status ddct_status = ddca_get_display_ref(ddct_did, &ddct_dref);
   if (ddct_status == 0) {
      DdcgDisplayRef * ddcg_dref = ddcg_display_ref_new();
      _ddcg_display_ref_set_ddct_object(ddcg_dref, ddct_dref);
      // also save context?
   }
   else {
      GQuark domain = g_quark_from_string("DDCTOOL_DDCG");
      g_set_error(error,  domain, ddct_status,
                  "invalid display identifier.  ddct_get_display_ref() returned ddct_status=%d", ddct_status);
   }
   return ddcg_dref;
}

