/* ddc_read_capabilities.c
 *
 * Functions to obtain the capabilities string for a display.
 * These functions are in a separate source file to simplify
 * the acyclic graph of #includes within the ddc source directory.
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

/** \file */

#include <config.h>

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <string.h>
#include <time.h>
/** \endcond */

#include "util/data_structures.h"
#include "util/report_util.h"

#include "base/core.h"
#include "base/ddc_errno.h"

#ifdef USE_USB
#include "usb/usb_displays.h"
#endif

#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"

#include "ddc/ddc_read_capabilities.h"

// Direct writes to stdout/stderr: none


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
 *    retry_history         if non-null, collects retryable errors
 *
 * Returns:
 *   status code
 */
static Error_Info *
get_capabilities_buffer(
      Display_Handle * dh,
      Buffer**         ppCapabilitiesBuffer)
{
   Public_Status_Code psc;
   Error_Info * ddc_excp = NULL;

   ddc_excp = multi_part_read_with_retry(
               dh,
               DDC_PACKET_TYPE_CAPABILITIES_REQUEST,
               0x00,                       // no subtype for capabilities
               false,                      // !all_zero_response_ok
               ppCapabilitiesBuffer);
   Buffer * cap_buffer = *ppCapabilitiesBuffer;
   // psc = (ddc_excp) ? ddc_excp->psc: 0;
   psc = ERRINFO_STATUS(ddc_excp);
   assert(psc <= 0);
   if (psc == 0) {
      // trim trailing blanks and nulls
      int len = buffer_length(*ppCapabilitiesBuffer);
      while ( len > 0 ) {
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
   return ddc_excp;
}


/* Gets the capabilities string for a display.
 *
 * The value is cached as this is an expensive operation.
 *
 * Arguments:
 *   dh       display handle
 *   pcaps    location where to return pointer to capabilities string.
 *   retry_history if non-null, collects retryable errors
 *
 * Returns:
 *   status code
 *
 * The returned pointer points to a string that is part of the
 * display handle.  It should NOT be freed by the caller.
 */
Error_Info *
get_capabilities_string(
      Display_Handle * dh,
      char**           pcaps)
{
   assert(dh);
   assert(dh->dref);

   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;
   if (!dh->dref->capabilities_string) {
      if (dh->dref->io_mode == DDCA_IO_USB) {
#ifdef USE_USB
         // newly created string, can just  reference
         dh->dref->capabilities_string = usb_get_capabilities_string_by_display_handle(dh);
#else
         PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
      }
      else {
         Buffer * pcaps_buffer;
         ddc_excp = get_capabilities_buffer(dh, &pcaps_buffer);
         // psc = (ddc_excp) ? ddc_excp->psc : 0;
         psc = ERRINFO_STATUS(ddc_excp);
         if (psc == 0) {
            dh->dref->capabilities_string = strdup((char *) pcaps_buffer->bytes);
            buffer_free(pcaps_buffer,__func__);
         }
      }
   }
   *pcaps = dh->dref->capabilities_string;
   return ddc_excp;
}


Error_Info *
get_capabilities_string_by_dref(Display_Ref * dref, char **pcaps) {
   assert(dref);

   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;
   if (!dref->capabilities_string) {
      Display_Handle * dh = NULL;
      psc = ddc_open_display(dref, CALLOPT_NONE, &dh);
      if (psc == 0) {
         ddc_excp = get_capabilities_string(dh, &dref->capabilities_string);
         ddc_close_display(dh);
      }
      else
         ddc_excp = errinfo_new(psc, __func__);
   }
   *pcaps = dref->capabilities_string;
   return ddc_excp;
}


