/* ddcg_display_identifier.c
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

#include <errno.h>
#include <stddef.h>

#include "util/report_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"

#include "gobject_api/ddcg_gobjects.h"


typedef struct {
   DDCA_Display_Identifier ddct_did;
} DdcgDisplayIdentifierPrivate;

struct _DdcgDisplayIdentifier {
   GObject                        parent_instance;
   DdcgContext *                  pcontext;                // needed?
   DdcgDisplayIdentifierPrivate * priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (DdcgDisplayIdentifier, ddcg_display_identifier, G_TYPE_OBJECT);

static void ddcg_display_identifier_class_init(DdcgDisplayIdentifierClass * cls);
static void ddcg_display_identifier_init(DdcgDisplayIdentifier * did);
#ifdef UNUSED
static void ddcg_display_identifier_finalize(GObject * obj);
#endif

static void
ddcg_display_identifier_class_init(DdcgDisplayIdentifierClass * cls) {
   GObjectClass * object_class = G_OBJECT_CLASS(cls);
   DBGMSG("Starting. object_class=%p", object_class);
}

static void
ddcg_display_identifier_init(DdcgDisplayIdentifier * ddcg_did) {
   DBGMSG("Starting");
   // initialize the instance
   ddcg_did->priv = ddcg_display_identifier_get_instance_private(ddcg_did);
   // DBGMSG("Set ddcg_did->priv=%p", ddcg_did->priv);
}

// End of boilerplate

DDCA_Display_Identifier
_ddcg_display_identifier_get_ddct_object(DdcgDisplayIdentifier * ddcg_did) {
   g_return_val_if_fail( DDCG_IS_DISPLAY_IDENTIFIER(ddcg_did), NULL);
   return ddcg_did->priv->ddct_did;
}


/**
 *  ddcg_display_identifier_report:
 *  @ddcg_did: pointer to the #DdcgDisplayIdentifier instance to be reported
 *  @depth:    logical indentation depth
 *
 *  Writes a report of the specified #DdcgDisplayIdentifier instance to the current
 *  report destination.
 *
 *  Returns:  nothing
 */
void
ddcg_display_identifier_report(DdcgDisplayIdentifier * ddcg_did, int depth) {
   g_return_if_fail( DDCG_IS_DISPLAY_IDENTIFIER(ddcg_did) );
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_vstring(depth, "DdcgDisplayIdentifier at %p", ddcg_did);
   rpt_vstring(d1, "parent_instance: %p", ddcg_did->parent_instance);
   rpt_vstring(d1, "pcontext:        %p", ddcg_did->pcontext);
   rpt_vstring(d1, "priv:            %p", ddcg_did->priv);
   if (ddcg_did->priv) {
      rpt_vstring(d2, "ddct_did:     %p", ddcg_did->priv->ddct_did);
   }
}


/**
 *  ddcg_display_identifier_create_busno_identifier: (constructor):
 *  @busno:        display number
 *  @error: (out): location where to return #GError if failure
 *
 *  Creates a #DdcgDisplayIdentifier specifying an I2C bus number
 *
 *  Returns: (transfer full): newly created #DdcgDisplayIdentifier
 */
DdcgDisplayIdentifier*
ddcg_display_identifier_create_busno_identifier(
      gint32      busno,
      GError **   error)
{
   g_return_val_if_fail (error == NULL || *error == NULL, NULL);

   DdcgDisplayIdentifier * ddcg_did = NULL;
   DDCA_Display_Identifier ddct_did = NULL;
   DDCA_Status ddct_status = ddca_create_busno_display_identifier(busno, &ddct_did);
   if (ddct_status == 0) {
      ddcg_did = g_object_new(DDCG_TYPE_DISPLAY_IDENTIFIER, NULL);
      ddcg_did->priv->ddct_did = ddct_did;
      // ddcg_display_identifier_report(ddcg_did, 0);
   }
   else {
      GQuark domain = g_quark_from_string("DDCTOOL_DDCG");
      g_set_error(error,  domain, ddct_status,
                  "ddct_create_busno_display_identifier() returned %d=ddct_status", ddct_status);
   }
   return ddcg_did;
}


/**
 *  ddcg_display_identifier_create_adlno_identifier: (constructor):
 *  @adapter_index:     ADL adapter index
 *  @display_index:     ADL display index
 *  @error: (out): location where to return #GError if failure
 *
 *  Creates a #DdcgDisplayIdentifier specifying an I2C bus number
 *
 *  Returns: (transfer full): newly created #DdcgDisplayIdentifier
 */
DdcgDisplayIdentifier*
ddcg_display_identifier_create_adlno_identifier(
      gint32      adapter_index,
      gint32      display_index,
      gint32      busno,
      GError **   error)
{
   g_return_val_if_fail (error == NULL || *error == NULL, NULL);

   DdcgDisplayIdentifier * ddcg_did = NULL;
   DDCA_Display_Identifier ddct_did = NULL;
   DDCA_Status ddct_status = ddca_create_adlno_display_identifier(adapter_index, display_index, &ddct_did);
   if (ddct_status == 0) {
      ddcg_did = g_object_new(DDCG_TYPE_DISPLAY_IDENTIFIER, NULL);
      ddcg_did->priv->ddct_did = ddct_did;
      // ddcg_display_identifier_report(ddcg_did, 0);
   }
   else {
      GQuark domain = g_quark_from_string("DDCTOOL_DDCG");
      g_set_error(error,  domain, ddct_status,
                  "ddct_create_adlno_display_identifier() returned %d=ddct_status", ddct_status);
   }
   return ddcg_did;
}


/**
 *  ddcg_display_identifier_create_usb_identifier: (constructor):
 *  @bus:          bus number
 *  @device:       device number
 *  @error: (out): location where to return #GError if failure
 *
 *  Creates a #DdcgDisplayIdentifier using USB bus and device numbers
 *
 *  Returns: (transfer full): newly created #DdcgDisplayIdentifier
 */
DdcgDisplayIdentifier*
ddcg_display_identifier_create_usb_identifier(
      gint32      bus,
      gint32      device,
      GError **   error)
{
   g_return_val_if_fail (error == NULL || *error == NULL, NULL);

   DdcgDisplayIdentifier * ddcg_did = NULL;
   DDCA_Display_Identifier ddct_did = NULL;
   DDCA_Status ddct_status = ddca_create_usb_display_identifier(bus, device, &ddct_did);
   if (ddct_status == 0) {
      ddcg_did = g_object_new(DDCG_TYPE_DISPLAY_IDENTIFIER, NULL);
      ddcg_did->priv->ddct_did = ddct_did;
      // ddcg_display_identifier_report(ddcg_did, 0);
   }
   else {
      GQuark domain = g_quark_from_string("DDCTOOL_DDCG");
      g_set_error(error,  domain, ddct_status,
                  "ddct_create_usb_display_identifier() returned %d=ddct_status", ddct_status);
   }
   return ddcg_did;
}


/**
 *  ddcg_display_identifier_create_mfg_model_sn_identifier: (constructor):
 *  @mfg_id:       3 character manufacturer id
 *  @model:        model name
 *  @sn:           serial number string
 *  @error: (out): location where to return #GError if failure
 *
 *  Creates a #DdcgDisplayIdentifier using USB bus and device numbers
 *
 *  Returns: (transfer full): newly created #DdcgDisplayIdentifier
 */
DdcgDisplayIdentifier*
ddcg_display_identifier_create_mfg_model_sn_identifier(
      const gchar *  mfg_id,
      const gchar *  model,
      const gchar *  sn,
      GError **      error)
{
   g_return_val_if_fail (error == NULL || *error == NULL, NULL);

   DdcgDisplayIdentifier * ddcg_did = NULL;
   DDCA_Display_Identifier ddca_did = NULL;
   DDCA_Status ddca_status = ddca_create_mfg_model_sn_display_identifier(mfg_id, model, sn, &ddca_did);
   if (ddca_status == 0) {
      ddcg_did = g_object_new(DDCG_TYPE_DISPLAY_IDENTIFIER, NULL);
      ddcg_did->priv->ddct_did = ddca_did;
      // ddcg_display_identifier_report(ddcg_did, 0);
   }
   else {
      GQuark domain = g_quark_from_string("DDCTOOL_DDCG");
      g_set_error(error,  domain, ddca_status,
                  "ddca_create_model_sn_identifier() returned %d=ddct_status", ddca_status);
   }
   return ddcg_did;
}





/**
 *  ddcg_display_identifier_create_dispno_identifier: (constructor):
 *  @dispno:    display number
 *  @error: (out) : location where to return #GError if failure
 *
 *  Creates a #DdcgDisplayIdentifier specifying a display number
 *
 *  Returns: (transfer full): newly created #DdcgDisplayIdentifier
 */
DdcgDisplayIdentifier*
ddcg_display_identifier_create_dispno_identifier(
      gint32      dispno,
      GError **   error)
{
   g_return_val_if_fail (error == NULL || *error == NULL, NULL);

   DdcgDisplayIdentifier * ddcg_did = NULL;
   DDCA_Display_Identifier ddct_did = NULL;
   DDCA_Status ddct_status = ddca_create_dispno_display_identifier(dispno, &ddct_did);
   if (ddct_status == 0) {
      ddcg_did = g_object_new(DDCG_TYPE_DISPLAY_IDENTIFIER, NULL);
      ddcg_did->priv->ddct_did = ddct_did;
      // ddcg_display_identifier_report(ddcg_did, 0);
   }
   else {
      GQuark domain = g_quark_from_string("DDCTOOL_DDCG");
      g_set_error(error,  domain, ddct_status,
                  "ddct_create_busno_display_identifier() returned %d=ddct_status", ddct_status);
   }
   return ddcg_did;
}


/**
 *  ddcg_display_identifier_repr:
 *  @ddcg_did:  points to the current #DdcgDisplayIdentifier instance
 *  @error: (out): location where to return #GError if failure
 *
 *  Creates a printable representation of the current instance.
 *
 *  Returns:  (transfer none): printable representation
 */
gchar *
ddcg_display_identifier_repr(
      DdcgDisplayIdentifier * ddcg_did,
      GError**                error)
{
   // DBGMSG("Starting. ddcg_did=%p", ddcg_did);
   g_return_val_if_fail (error == NULL || *error == NULL, NULL);
   g_return_val_if_fail( DDCG_IS_DISPLAY_IDENTIFIER(ddcg_did), NULL);

   char * repr = ddca_did_repr(ddcg_did->priv->ddct_did);
   if (!repr) {
      GQuark domain = g_quark_from_string("DDCTOOL_DDCG");
      g_set_error(error,  domain, DDCL_ARG, "ddct_repr_identifier() returned %d=ddct_status", DDCL_ARG);
   }
   // DBGMSG("Returning %p -> |%s|", repr, repr);
   // solves the problem of free failure in python runtime,
   // but why was python trying to free result if transfer mode = none?
   char * s = g_strdup(repr);
   // DBGMSG("Returning %p -> |%s|", s, s);
   return s;
}




#ifdef UNUSED
static void ddcg_display_identifier_finalize(GObject * obj) {
}
#endif


#ifdef NO_SUBCLASS

G_DEFINE_TYPE (DdcgBusnoDisplayIdentifier, ddcg_busno_display_identifier, G_TYPE_OBJECT);


static void ddcg_busno_display_identifier_class_init(DdcgBusnoDisplayIdentifierClass * cls);
static void ddcg_busno_display_identifier_init(DdcgBusnoDisplayIdentifier * did);
#ifdef UNUSED
static void ddcg_busno_display_identifier_finalize(GObject * obj);
#endif


static void ddcg_busno_display_identifier_class_init(DdcgBusnoDisplayIdentifierClass * cls) {
}

static void ddcg_busno_display_identifier_init(DdcgBusnoDisplayIdentifier * did) {
}

#ifdef UNUSED
static void ddcg_busno_display_identifier_finalize(GObject * obj) {
}
#endif

GObject * busno_display_identifier_new(void) {
   return g_object_new(DDCG_TYPE_BUSNO_DISPLAY_IDENTIFIER, 0);
}

#endif

