/* ddc_vcp_version.c
 *
 * Created on: Dec 31, 2015
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

#include <stdbool.h>

#include "base/msg_control.h"
#include "base/status_code_mgt.h"

#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_vcp.h"

#include "ddc/ddc_vcp_version.h"



//
// Functions for VCP (MCCS) version
//

/* Gets the VCP version.
 *
 * Because the VCP version is used repeatedly for interpreting other
 * VCP feature values, it is cached.
 *
 * Arguments:
 *    dh     display handle
 *
 * Returns:
 *    Version_Spec struct containing version, contains 0.0 if version
 *    could not be retrieved (pre MCCS v2)
 */
Version_Spec get_vcp_version_by_display_handle(Display_Handle * dh) {
   bool debug = false;
   // printf("(%s) Starting. dh=%p, dh->vcp_version =  %d.%d\n",
   //        __func__, dh, dh->vcp_version.major, dh->vcp_version.minor);
   if (is_version_unqueried(dh->vcp_version)) {
      dh->vcp_version.major = 0;
      dh->vcp_version.minor = 0;
      Preparsed_Nontable_Vcp_Response * pinterpreted_code;

      // verbose output is distracting since this function is called when
      // querying for other things
      Output_Level olev = get_output_level();
      if (olev == OL_VERBOSE)
         set_output_level(OL_NORMAL);
      Global_Status_Code  gsc = get_nontable_vcp_value_by_display_handle(dh, 0xdf, &pinterpreted_code);
      if (olev == OL_VERBOSE)
         set_output_level(olev);
      if (gsc == 0) {
         dh->vcp_version.major = pinterpreted_code->sh;
         dh->vcp_version.minor = pinterpreted_code->sl;
      }
      else {
         // happens for pre MCCS v2 monitors
         DBGMSF(debug, "Error detecting VCP version. gsc=%s\n", gsc_desc(gsc) );
      }
   }
   // DBGMSG("Returning: %d.%d", dh->vcp_version.major, dh->vcp_version.minor);
   return dh->vcp_version;
}


/* Gets the VCP version.
 *
 * Because the VCP version is used repeatedly for interpreting other
 * VCP feature values, it is cached.
 *
 * Arguments:
 *    dref     display reference
 *
 * Returns:
 *    Version_Spec struct containing version, contains 0.0 if version
 *    could not be retrieved (pre MCCS v2)
 */
Version_Spec get_vcp_version_by_display_ref(Display_Ref * dref) {
   // printf("(%s) Starting. dref=%p, dref->vcp_version =  %d.%d\n",
   //        __func__, dref, dref->vcp_version.major, dref->vcp_version.minor);

   if (is_version_unqueried(dref->vcp_version)) {
      Display_Handle * dh = ddc_open_display(dref, EXIT_IF_FAILURE);
      dref->vcp_version = get_vcp_version_by_display_handle(dh);
      ddc_close_display(dh);
   }

   // DBGMSG("Returning: %d.%d", dref->vcp_version.major, vspec.minor);
   return dref->vcp_version;
}

