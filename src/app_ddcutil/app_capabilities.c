// app_capabilities.c

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 

// TEMPORARILY EVERYTHING FROM MAIN


/** \cond */
#include <config.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/data_structures.h"
#include "util/error_info.h"
#include "util/failsim.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/sysfs_util.h"
/** \endcond */

#include "public/ddcutil_types.h"

#include "base/adl_errors.h"
#include "base/base_init.h"
#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/dynamic_sleep.h"
#include "base/linux_errno.h"
#include "base/parms.h"
#include "base/sleep.h"
#include "base/status_code_mgt.h"
#include "base/thread_retry_data.h"
#include "base/tuned_sleep.h"
#include "base/thread_sleep_data.h"

#include "vcp/parse_capabilities.h"
#include "vcp/vcp_feature_codes.h"

#include "dynvcp/dyn_dynamic_features.h"
#include "dynvcp/dyn_parsed_capabilities.h"

#include "i2c/i2c_bus_core.h"
#include "i2c/i2c_strategy_dispatcher.h"
#include "adl/adl_shim.h"

#include "usb/usb_displays.h"

#include "ddc/ddc_displays.h"
#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_output.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_services.h"
#include "ddc/ddc_try_stats.h"
#include "ddc/ddc_vcp_version.h"
#include "ddc/ddc_vcp.h"

#include "cmdline/cmd_parser_aux.h"    // for parse_feature_id_or_subset(), should it be elsewhere?
#include "cmdline/cmd_parser.h"
#include "cmdline/parsed_cmd.h"

#include "app_ddcutil/app_dynamic_features.h"
#include "app_ddcutil/app_dumpload.h"
#include "app_ddcutil/app_getvcp.h"
#include "app_ddcutil/app_setvcp.h"

#include "app_sysenv/query_sysenv.h"
#ifdef USE_USB
#include "app_sysenv/query_sysenv_usb.h"
#endif

#ifdef INCLUDE_TESTCASES
#include "test/testcases.h"
#endif

#ifdef USE_API
#include "public/ddcutil_c_api.h"
#endif


#include "app_ddcutil/app_capabilities.h"

// Default race class for this file
// static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_TOP;





// TODO: refactor
//       originally just displayed capabilities, now returns parsed capabilities as well
//       these actions should be separated
Parsed_Capabilities *
perform_get_capabilities_by_display_handle(Display_Handle * dh) {
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




void perform_show_parsed_capabilities(char * capabilities_string, Display_Handle * dh, Parsed_Capabilities * pcap) {
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


