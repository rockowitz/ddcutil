/* ddc_display_handle.c
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

#include <errno.h>

#include "../libmain/ddcutil_c_api.h"
#include "base/core.h"

#include "gobject_api/ddcg_gobjects.h"


typedef struct {
   // whatever
   DDCA_Display_Handle ddct_dh;
} DdcgDisplayHandlePrivate;



struct _DdcgDisplayHandle {
   GObject   parent_instance;

   // class instance variables go here
   DdcgDisplayHandlePrivate* priv;
};


G_DEFINE_TYPE_WITH_PRIVATE(DdcgDisplayHandle, ddcg_display_handle, G_TYPE_OBJECT);

#ifdef FORWARE_REF_NOT_NEEDED
static void ddcg_display_handle_class_init(DdcgDisplayHandleClass * cls);
static void ddcg_display_handle_init(DdcgDisplayHandle * display_handle);
#endif
#ifdef UNUSED
static void ddcg_display_handle_finalize(GObject * obj);
#endif



static void ddcg_display_handle_class_init(DdcgDisplayHandleClass * cls) {
   DBGMSG("Starting");
}


static void ddcg_display_handle_init(DdcgDisplayHandle * ddcg_dh) {
   DBGMSG("Starting");
   // initialize the instance
   ddcg_dh->priv = ddcg_display_handle_get_instance_private(ddcg_dh);

}

#ifdef UNUSED
static void ddcg_display_handle_finalize(GObject * obj) {
}
#endif

DdcgDisplayHandle * ddcg_display_handle_new(void) {
   return g_object_new(DDCG_TYPE_DISPLAY_HANDLE, NULL);
}



/**
 * ddcg_display_handle_open0:
 * @ddcg_dref:        a #DdcgDisplayRef indicating the device to open
 * @pddcg_dh: (out):   location where to return pointer  #DdcgDisplayHandle
 *                    representing the opened device
 *
 * Opens a display for reading and writing.
 *
 * Returns:  status code
 */
DdcgStatusCode
ddcg_display_handle_open0(DdcgDisplayRef * ddcg_dref, DdcgDisplayHandle ** pddcg_dh) {
   g_return_val_if_fail( DDCG_IS_DISPLAY_REF(ddcg_dref), -EINVAL);

   DdcgStatusCode result = 0;
   DDCA_Display_Ref ddct_dref = _ddcg_display_ref_get_ddct_object(ddcg_dref);
   DDCA_Display_Handle ddct_dh = NULL;
   DDCA_Status ddct_status = ddca_open_display(ddct_dref, &ddct_dh);
   if (ddct_status == 0) {
      DdcgDisplayHandle * ddcg_dh = ddcg_display_handle_new();
      ddcg_dh->priv->ddct_dh = ddct_dh;
      *pddcg_dh = ddcg_dh;
   }
   else {
      *pddcg_dh = NULL;
      result = ddct_status;      // temp
   }
   return result;
}


/**
 * ddcg_display_handle_open:
 * @ddcg_dref:        a #DdcgDisplayRef indicating the device to open
 * @error: (out):   location where to return pointer  #GEerror if error
 *
 * Opens a display for reading and writing.
 *
 * Returns: (transfer full):
 */
DdcgDisplayHandle *
ddcg_display_handle_open(DdcgDisplayRef * ddcg_dref, GError ** error) {
   g_return_val_if_fail (error == NULL || *error == NULL, NULL);

   DdcgDisplayHandle * ddcg_dh = NULL;
   DDCA_Display_Handle ddct_dh = NULL;
   DDCA_Display_Ref ddct_dref = _ddcg_display_ref_get_ddct_object(ddcg_dref);
   DDCA_Status ddct_status = ddca_open_display(ddct_dref, &ddct_dh);
   if (ddct_status == 0) {
      ddcg_dh = ddcg_display_handle_new();
      ddcg_dh->priv->ddct_dh = ddct_dh;
   }
   else {
      GQuark domain = g_quark_from_string("DDCTOOL_DDCG");
      g_set_error(error,  domain, ddct_status, "ddct_open_display() returned ddct_status=%d", ddct_status);
   }
   return ddcg_dh;
}



/**
 * ddcg_display_handle_close:
 * @ddcg_dh:        a #DdcgDisplayHandle indicating the device to close
 *
 * Closes a device
 *
 * Returns:  status code
 */
DdcgStatusCode
ddcg_display_handle_close(DdcgDisplayHandle * ddcg_dh) {
   DDCA_Status ddct_status  = ddca_close_display(ddcg_dh->priv->ddct_dh);
   DdcgStatusCode ddcg_status = ddct_status;     // TODO: replace with function
   return ddcg_status;
}


/**
 * ddcg_display_handle_get_nontable_vcp_value:
 * @ddcg_dh:        a #DdcgDisplayHandle indicating the current instance
 * @feature_code:    VCP feature code
 * @error: (out):   location where to return pointer  #GEerror if error
 *
 * Retrieve a raw non-table VCP feature value
 *
 * Returns:  (transfer full): point to #DdcgContRespose
 */
DdcgContResponse *
ddcg_display_handle_get_nontable_vcp_value(
               DdcgDisplayHandle *             ddcg_dh,
               DdcgFeatureCode               feature_code,
               GError **             error)
{
   DdcgContResponse * ddcg_response = NULL;
   DDCA_Non_Table_Value_Response ddct_response;

   DDCA_Status ddct_status =  ddca_get_nontable_vcp_value(
                  ddcg_dh->priv->ddct_dh,
                  feature_code,
                  &ddct_response);
   // DBGMSG("ddct_status = %d", ddct_status);
   if (ddct_status == 0) {
      // allocate a new DdcgContResponse instance
      ddcg_response = g_object_new(DDCG_TYPE_CONT_RESPONSE, NULL);

      // or set properties?
      ddcg_response->mh = ddct_response.mh;
      ddcg_response->ml = ddct_response.ml;
      ddcg_response->sh = ddct_response.sh;
      ddcg_response->sl = ddct_response.sl;
      ddcg_response->cur_value = ddct_response.cur_value;
      ddcg_response->max_value = ddct_response.max_value;
      // ddcg_cont_response_report(ddcg_response, 1);
   }
   else {
      GQuark domain = g_quark_from_string("DDCTOOL_DDCG");
      g_set_error(error,  domain, ddct_status, "ddct_get_nontable_vcp_value() returned ddct_status=%d", ddct_status);
   }

   // DBGMSG("Returning ddcg_response=%p", ddcg_response);
   return ddcg_response;
}


/**
 * ddcg_display_handle_repr:
 * @ddcg_dh:        a #DdcgDisplayHandle indicating the current instance
 * @error: (out callee-allocates):      location where to return error information
 *
 * Returns a brief description of the current instance
 *
 * Returns:  (transfer none): status code
 */
gchar *
ddcg_display_handle_repr(
      DdcgDisplayHandle *  ddcg_dh,
      GError **              error)
{
   g_return_val_if_fail( DDCG_IS_DISPLAY_HANDLE(ddcg_dh), NULL);

   gchar * repr = NULL;
   DDCA_Status ddct_status = ddca_repr_display_handle(
              ddcg_dh->priv->ddct_dh, &repr);
   // DBGMSG("repr=%p", repr);
   // DBGMSG("repr = %s", repr);
   if (ddct_status == 0) {
   }
   else {
      GQuark domain = g_quark_from_string("DDCTOOL_DDCG");
      g_set_error(error,  domain, ddct_status, "ddct_repr_display_handle() returned ddct_status=%d", ddct_status);
   }

   return g_strdup(repr);
}



#ifdef REF
TO IMPLEMENT:

DDCA_Status ddct_get_mccs_version(DDCA_Display_Handle ddct_dh, DDCT_MCCS_Version_Spec* pspec);


#endif


