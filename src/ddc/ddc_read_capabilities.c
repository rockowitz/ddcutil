/** \file ddc_read_capabilities.c */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config.h>

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <string.h>
#include <time.h>
/** \endcond */

#include "util/data_structures.h"
#include "util/report_util.h"

#include "public/ddcutil_types.h"

#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/rtti.h"
#include "base/sleep.h"
#include "base/tuned_sleep.h"

#ifdef USE_USB
#include "usb/usb_displays.h"
#endif

#include "vcp/persistent_capabilities.h"

#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"

#include "ddc/ddc_read_capabilities.h"

// Direct writes to stdout/stderr: none

// Default trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDC;


/** Executes the VCP Get Capabilities command to obtain the
 *  capabilities string.  The string is returned in null terminated
 *  form in a Buffer struct.  It is the responsibility of the caller to
 *  free this struct.
 *
 * @param dh                       display handle
 * @param capabilities_buffer_loc  address at which to return pointer to allocated Buffer
 * @return                         status code
 */
static Error_Info *
get_capabilities_into_buffer(
      Display_Handle * dh,
      Buffer**         capbilities_buffer_loc)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dh=%s", dh_repr_t(dh));
   Public_Status_Code psc;
   Error_Info * ddc_excp = NULL;

   TUNED_SLEEP_WITH_TRACE(dh, SE_PRE_MULTI_PART_READ, "Before reading capabilities");

   ddc_excp = multi_part_read_with_retry(
               dh,
               DDC_PACKET_TYPE_CAPABILITIES_REQUEST,
               0x00,                       // no subtype for capabilities
               false,                      // !all_zero_response_ok
               capbilities_buffer_loc);
   Buffer * cap_buffer = *capbilities_buffer_loc;
   // psc = (ddc_excp) ? ddc_excp->psc: 0;
   psc = ERRINFO_STATUS(ddc_excp);
   assert(psc <= 0);
   if (psc == 0) {
      // trim trailing blanks and nulls
      int len = buffer_length(*capbilities_buffer_loc);
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
   DBGMSF(debug, "Done.     dh=%s, Returning: %s", dh_repr_t(dh), errinfo_summary(ddc_excp));
   return ddc_excp;
}


/* Gets the capabilities string for a display.
 *
 * The value is cached as this is an expensive operation.
 *
 * Arguments:
 *   dh       display handle
 *   caps_loc location where to return pointer to capabilities string.
 *
 * Returns:
 *   status code
 *
 * The returned pointer points to a string that is part of the
 * display handle.  It should NOT be freed by the caller.
 */
Error_Info *
ddc_get_capabilities_string(
      Display_Handle * dh,
      char**           caps_loc)
{
   bool debug = false;
   assert(dh);
   assert(dh->dref);
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s", dh_repr_t(dh));

   // Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;
   if (!dh->dref->capabilities_string) {
      if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
#ifdef USE_USB
         // newly created string, can just  reference
         dh->dref->capabilities_string = usb_get_capabilities_string_by_dh(dh);
#else
         PROGRAM_LOGIC_ERROR("ddcutil not built with USB support");
#endif
      }
      else {
         // n. persistent_capabilities_enabled handled in get_persistent_capabilities()
         dh->dref->capabilities_string = get_persistent_capabilities(dh->dref->mmid);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "get_persistent_capabilities() returned |%s|",
                                    dh->dref->capabilities_string);
         if (dh->dref->capabilities_string && get_output_level() >= DDCA_OL_VERBOSE) {
            char * s = get_capabilities_cache_file_name();
            rpt_vstring(0, "Read cached capabilities string from %s", s);
            free(s);
         }

         if (!dh->dref->capabilities_string) {
            Buffer * pcaps_buffer;
            ddc_excp = get_capabilities_into_buffer(dh, &pcaps_buffer);
            if (!ddc_excp) {
               dh->dref->capabilities_string = strdup((char *) pcaps_buffer->bytes);
               buffer_free(pcaps_buffer,__func__);
               set_persistent_capabilites(dh->dref->mmid, dh->dref->capabilities_string);
            }
         }
      }
   }
   *caps_loc = dh->dref->capabilities_string;
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %s. *caps_loc -> |%s|",
                                   errinfo_summary(ddc_excp), *caps_loc);
   return ddc_excp;
}


#ifdef UNUSED
Error_Info *
get_capabilities_string_by_dref(Display_Ref * dref, char **pcaps) {
   assert(dref);
   bool debug = false;
   DBGMSF(debug, "Starting. dref=%s", dref_repr_t(dref));

   Public_Status_Code psc = 0;
   Error_Info * ddc_excp = NULL;
   if (!dref->capabilities_string) {
      Display_Handle * dh = NULL;
      psc = ddc_open_display(dref, CALLOPT_NONE, &dh);
      if (psc == 0) {
         ddc_excp = ddc_get_capabilities_string(dh, &dref->capabilities_string);
         int rc = ERRINFO_STATUS(ddc_excp);
         if (rc == -EBADF) {
            DBGMSF(debug, "EBADF, skipping ddc_close_display()");
         }
         else
            ddc_close_display(dh);
      }
      else
         ddc_excp = errinfo_new(psc, __func__);
   }
   *pcaps = dref->capabilities_string;
   DBGMSF(debug, "Done.    Returning: %p", ddc_excp);
   DBGMSF(debug, "Done.     dref = %s, error_info = %s, capabilities: %s", dref_repr_t(dref),
          errinfo_summary(ddc_excp), *pcaps);
   return ddc_excp;
}
#endif


void init_ddc_read_capabilities() {
   RTTI_ADD_FUNC(ddc_get_capabilities_string);
}

