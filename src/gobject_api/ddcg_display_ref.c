/* ddcg_display_ref.c
 *
 * Created on: Jan 13, 2016
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "util/report_util.h"
#include "base/msg_control.h"
#include "libmain/ddct_public.h"

#include "gobject_api/ddcg_gobjects.h"


typedef struct {
   DDCT_Display_Ref ddct_dref;
} DdcgDisplayRefPrivate;


struct _DdcgDisplayRef {
   GObject                 parent_instance;
   DdcgContext *           pcontext;   // needed?
   DdcgDisplayRefPrivate * priv;
};


G_DEFINE_TYPE_WITH_PRIVATE(DdcgDisplayRef, ddcg_display_ref, G_TYPE_OBJECT);

#ifdef UNNEEDED_FORWARD_REFS
static void ddcg_display_ref_class_init(DdcgDisplayRefClass * cls);
static void ddcg_display_ref_init(DdcgDisplayRef * display_ref);
#endif


static void
ddcg_display_ref_class_init(DdcgDisplayRefClass * cls) {
   DBGMSG("Starting");
#ifdef WONT_COMPILE
   GObjectClass * object_class = G_OBJECT_CLASS(cls);
   object_class->constructed = ddcg_display_ref_constructed;
#endif
}


static void
ddcg_display_ref_init(DdcgDisplayRef * ddcg_dref) {
   DBGMSG("Starting");
   // initialize the instance
   ddcg_dref->priv = ddcg_display_ref_get_instance_private(ddcg_dref);
}


DdcgDisplayRef *
ddcg_display_ref_new(void) {
   return g_object_new(DDCG_TYPE_DISPLAY_REF, NULL);
}

// end of boilerplate


void
_ddcg_display_ref_set_ddct_object(
      DdcgDisplayRef * ddcg_dref,
      DDCT_Display_Ref ddct_dref)
{
   g_return_if_fail( DDCG_IS_DISPLAY_REF(ddcg_dref) );
   ddcg_dref->priv->ddct_dref = ddct_dref;
}



DDCT_Display_Ref
_ddcg_display_ref_get_ddct_object(DdcgDisplayRef * ddcg_dref) {
   g_return_val_if_fail( DDCG_IS_DISPLAY_REF(ddcg_dref), NULL);
   return ddcg_dref->priv->ddct_dref;
}


/**
 * ddcg_display_ref_get:
 * @ddcg_did:   a #DdcgDisplayRef pointing to the current instance
 * @error:  (out) : location where to return pointer to created GError if failure
 *
 * Creates a #DcggDisplayRef.
 *
 * Returns: (transfer full) : pointer to newly created #DdcgDisplayRef, NULL if failure
 */
DdcgDisplayRef *
ddcg_display_ref_get(DdcgDisplayIdentifier * ddcg_did, GError ** error) {
   g_return_val_if_fail (error == NULL || *error == NULL, NULL);

   DdcgDisplayRef * ddcg_dref = NULL;
   DDCT_Display_Ref ddct_dref = NULL;
   DDCT_Display_Identifier ddct_did = _ddcg_display_identifier_get_ddct_object(ddcg_did);
   assert(ddcg_did);
   DDCT_Status ddct_status = ddct_get_display_ref( ddct_did, &ddct_dref);
   if (ddct_status == 0) {
      ddcg_dref = g_object_new(DDCG_TYPE_DISPLAY_REF, NULL);
      // DBGMSG("ddcg_dref=%p", ddcg_dref);
      // DBGMSG("ddcg_dref->priv=%p", ddcg_dref->priv);
      ddcg_dref->priv->ddct_dref = ddct_dref;
      // ddcg_display_ref_report(ddcg_dref, 0);
   }
     else {
      GQuark domain = g_quark_from_string("DDCTOOL_DDCG");
      g_set_error(error,  domain, ddct_status, "ddct_get_display_ref() returned %d=ddcg_status", ddct_status);
   }
   return ddcg_dref;
}



/**
 *  ddcg_display_ref_repr:
 *  @ddcg_dref: a #DdcgDisplayRef
 *  @error:  (out) : location where to return pointer to created GError if failure
 *
 *  Creates a printable representation of the current instance.
 *
 *  Returns: (transfer none):  printable representation of the current instance
 */
gchar *
ddcg_display_ref_repr(
      DdcgDisplayRef *  ddcg_dref,
      GError **         error)
{
   g_return_val_if_fail (error == NULL || *error == NULL, NULL);
   g_return_val_if_fail( DDCG_IS_DISPLAY_REF(ddcg_dref), NULL);

   gchar * repr = NULL;
   DDCT_Status ddcg_status = ddct_repr_display_ref(
                                ddcg_dref->priv->ddct_dref, &repr);
   if (ddcg_status != 0) {
      GQuark domain = g_quark_from_string("DDCTOOL_DDCG");
      g_set_error(error,  domain, ddcg_status, "ddcg_display_ref_repr() returned %d=ddcg_status", ddcg_status);
   }
   return g_strdup(repr);  // w/o g_strdup get free(): invalid pointer in Python
}




/**
 *  ddcg_display_ref_report:
 *  @ddcg_dref: a #DdcgDisplayRef
 *  @depth: logical indentation depth
 *
 *  Report on the specified instance.
 *
 *  Returns:  nothing
 */
void
ddcg_display_ref_report(
      DdcgDisplayRef * ddcg_dref,
      int              depth)
{
   g_return_if_fail( DDCG_IS_DISPLAY_REF(ddcg_dref) );
   rpt_vstring(depth, "DdcgDisplayRef at %p:", ddcg_dref);
   ddct_report_display_ref(ddcg_dref->priv->ddct_dref, depth+1);
}





