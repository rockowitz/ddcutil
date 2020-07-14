/** \file app_capabilities.c
 *
 *  Capabilities functions factored out of main.c
 */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <config.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/error_info.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "public/ddcutil_types.h"

#include "base/core.h"
#include "base/displays.h"
/** \endcond */

#include "vcp/parse_capabilities.h"

#include "dynvcp/dyn_parsed_capabilities.h"

#include "ddc/ddc_read_capabilities.h"

#include "app_ddcutil/app_capabilities.h"

// Default trace class for this file
// static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_TOP;


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

DDCA_Status app_get_capabilities_string(Display_Handle * dh, char ** capabilities_string_loc) {
   // FILE * fout = stdout;
    FILE * ferr = stderr;
    bool debug = false;
    Error_Info * ddc_excp = get_capabilities_string(dh, capabilities_string_loc);
    Public_Status_Code psc =  ERRINFO_STATUS(ddc_excp);
    assert( (ddc_excp && psc!=0) || (!ddc_excp && psc==0) );

    if (ddc_excp) {
       switch(psc) {
       case DDCRC_REPORTED_UNSUPPORTED:       // should not happen
       case DDCRC_DETERMINED_UNSUPPORTED:
          f0printf(ferr, "Unsupported request\n");
          break;
       case DDCRC_RETRIES:
          f0printf(ferr, "Unable to get capabilities for monitor on %s.  Maximum DDC retries exceeded.\n",
                  dh_repr(dh));
          break;
       default:
          f0printf(ferr, "(%s) !!! Unable to get capabilities for monitor on %s\n",
                 __func__, dh_repr(dh));
          DBGMSG("Unexpected status code: %s", psc_desc(psc));
       }
       // errinfo_free(ddc_excp);
       ERRINFO_FREE_WITH_REPORT(ddc_excp, debug || report_freed_exceptions);
    }
    else {
       assert(capabilities_string_loc);
    }
    return psc;
}




Parsed_Capabilities *
app_get_capabilities_by_display_handle(Display_Handle * dh) {
   // FILE * fout = stdout;
   FILE * ferr = stderr;
   bool debug = false;
   Parsed_Capabilities * pcap = NULL;
   char * capabilities_string;
   Error_Info * ddc_excp = get_capabilities_string(dh, &capabilities_string);
   Public_Status_Code psc =  ERRINFO_STATUS(ddc_excp);
   assert( (ddc_excp && psc!=0) || (!ddc_excp && psc==0) );

   if (ddc_excp) {
      switch(psc) {
      case DDCRC_REPORTED_UNSUPPORTED:       // should not happen
      case DDCRC_DETERMINED_UNSUPPORTED:
         f0printf(ferr, "Unsupported request\n");
         break;
      case DDCRC_RETRIES:
         f0printf(ferr, "Unable to get capabilities for monitor on %s.  Maximum DDC retries exceeded.\n",
                 dh_repr(dh));
         break;
      default:
         f0printf(ferr, "(%s) !!! Unable to get capabilities for monitor on %s\n",
                __func__, dh_repr(dh));
         DBGMSG("Unexpected status code: %s", psc_desc(psc));
      }
      // errinfo_free(ddc_excp);
      ERRINFO_FREE_WITH_REPORT(ddc_excp, debug || report_freed_exceptions);
   }
   else {
      assert(capabilities_string);
      // pcap is always set, but may be damaged if there was a parsing error
      pcap = parse_capabilities_string(capabilities_string);
#ifdef OUT
      DDCA_Output_Level output_level = get_output_level();
      if (output_level <= DDCA_OL_TERSE) {
         f0printf(fout,
                  "%s capabilities string: %s\n",
                  (dh->dref->io_path.io_mode == DDCA_IO_USB) ? "Synthesized unparsed" : "Unparsed",
                  capabilities_string);
      }
      else {
         if (dh->dref->io_path.io_mode == DDCA_IO_USB)
            pcap->raw_value_synthesized = true;
         // report_parsed_capabilities(pcap, dh->dref->io_path.io_mode);    // io_mode no longer needed
         dyn_report_parsed_capabilities(
               pcap,
               dh,
               NULL,
               0);
         // free_parsed_capabilities(pcap);
      }
#endif
   }
   DBGMSF(debug, "Returning: %p", pcap);
   return pcap;
}


void app_show_parsed_capabilities2(Display_Handle * dh, Parsed_Capabilities * pcap) {
   assert(pcap);


      if ( dh->dref->io_path.io_mode == DDCA_IO_USB)
         pcap->raw_value_synthesized = true;

      // report_parsed_capabilities(pcap, dh->dref->io_path.io_mode);    // io_mode no longer needed
      dyn_report_parsed_capabilities(
           pcap,
           dh,
           NULL,
           0);
      // free_parsed_capabilities(pcap);
}


void app_show_parsed_capabilities(char * capabilities_string, Display_Handle * dh, Parsed_Capabilities * pcap) {
   assert(pcap);
   FILE * fout = stdout;
   DDCA_Output_Level output_level = get_output_level();
   if (output_level <= DDCA_OL_TERSE) {
      f0printf(fout,
               "%s capabilities string: %s\n",
                    (dh->dref->io_path.io_mode == DDCA_IO_USB) ? "Synthesized unparsed" : "Unparsed",
               capabilities_string);
   }
   else {
      if ( dh->dref->io_path.io_mode == DDCA_IO_USB)
         pcap->raw_value_synthesized = true;

      // report_parsed_capabilities(pcap, dh->dref->io_path.io_mode);    // io_mode no longer needed
      dyn_report_parsed_capabilities(
           pcap,
           dh,
           NULL,
           0);
      // free_parsed_capabilities(pcap);
   }
}


DDCA_Status app_capabilities(Display_Handle * dh) {

   char * capabilities_string;
   DDCA_Status ddcrc;
   FILE * fout = stdout;

   ddcrc = app_get_capabilities_string(dh, &capabilities_string);
   if (ddcrc == 0) {
      DDCA_Output_Level ol = get_output_level();
      if (ol == DDCA_OL_TERSE) {
          f0printf(fout,
                  "%s capabilities string: %s\n",
                       (dh->dref->io_path.io_mode == DDCA_IO_USB) ? "Synthesized unparsed" : "Unparsed",
                  capabilities_string);
      }
      else {
         // pcaps is always set, but may be damaged if there was a parsing error
         Parsed_Capabilities * pcaps = parse_capabilities_string(capabilities_string);
         app_show_parsed_capabilities2(dh, pcaps);
         free_parsed_capabilities(pcaps);
      }

   }
   return ddcrc;

}
