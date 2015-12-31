/* ddc_read_capabilities.c
 *
 * Created on: Dec 28, 2015
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
#include <glib.h>
#include <string.h>
#include <time.h>

#include "util/report_util.h"

#include "base/ddc_errno.h"
// #include "base/ddc_packets.h"
// #include "base/displays.h"
// #include "base/linux_errno.h"
#include "base/msg_control.h"
// #include "base/parms.h"


#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"

#include "ddc/ddc_read_capabilities.h"


//
// Capabilities Related Functions
//

/* Executes the VCP Get Capabilities command to obtain the
 * capabilities string.  The string is returned in null terminated
 * form in a Buffer struct.  It is the responsibility of the caller to
 * free this struct.
 *
 * Arguments:
 *    dh                    pointer to display handle
 *    ppCapabilitiesBuffer  address at which to return pointer to allocated Buffer
 *
 * Returns:
 *   status code
 */
Global_Status_Code get_capabilities_buffer_by_display_handle(
                      Display_Handle * dh,
                      Buffer**         ppCapabilitiesBuffer) {
   int rc;

   rc = multi_part_read_with_retry(
           dh,
           DDC_PACKET_TYPE_CAPABILITIES_REQUEST,
           0x00,                       // no subtype for capabilities
           ppCapabilitiesBuffer);

   Buffer * cap_buffer = *ppCapabilitiesBuffer;
   if (rc >= 0) {
      // trim trailing blanks and nulls
      int len = buffer_length(*ppCapabilitiesBuffer);
      while ( len > 0) {
         Byte ch = cap_buffer->bytes[len-1];
         if (ch == ' ' || ch == '\0')
            len--;
         else
            break;
      }
      // since buffer contains a string, put a single null at end
      buffer_set_byte(cap_buffer, len, '\0');
      buffer_set_length(cap_buffer, len+1);
   }
   return rc;
}


Global_Status_Code get_capabilities_string_by_display_handle(Display_Handle * dh, char** pcaps) {
   Global_Status_Code gsc = 0;
   if (!dh->capabilities_string) {
      Buffer * pcaps_buffer;
      gsc = get_capabilities_buffer_by_display_handle(dh, &pcaps_buffer);
      if (gsc == 0) {
         dh->capabilities_string = strdup((char *) pcaps_buffer->bytes);
         buffer_free(pcaps_buffer,__func__);
      }
   }
   *pcaps = dh->capabilities_string;
   return gsc;
}


/* Executes the VCP Get Capabilities command to obtain the
 * capabilities string.  The string is returned in null terminated
 * form in a Buffer struct.  It is the responsibility of the caller to
 * free this struct.
 *
 * Arguments:
 *    dref                  pointer to display reference
 *    ppCapabilitiesBuffer  address at which to return pointer to allocated Buffer
 *
 * Returns:
 *   status code
 */
Global_Status_Code get_capabilities_buffer_by_display_ref(Display_Ref * dref, Buffer** ppCapabilitiesBuffer) {
   int rc;
   Display_Handle* dh = ddc_open_display(dref, EXIT_IF_FAILURE);
   rc = get_capabilities_buffer_by_display_handle(dh, ppCapabilitiesBuffer);
   ddc_close_display(dh);
   return rc;
}


Global_Status_Code get_capabilities_string_by_display_ref(Display_Ref * dref, char** pcaps) {
   int rc;
   Display_Handle* dh = ddc_open_display(dref, EXIT_IF_FAILURE);
   rc = get_capabilities_string_by_display_handle(dh, pcaps);
   ddc_close_display(dh);
   return rc;
}
