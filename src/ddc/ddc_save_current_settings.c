/** @file ddc_save_current_settings.c
 *  Implement DDC command Save Current Settings
 */

// Copyright (C) 2014-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>

#include "public/ddcutil_status_codes.h"

#include "util/error_info.h"

#include "base/core.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/rtti.h"

#include "ddc_packet_io.h"

#include "ddc_save_current_settings.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDC;

//
//  DDC command Save Current Settings
//

/** Executes the DDC Save Current Settings command.
 *
 * @param  dh handle of open display device
 * @return NULL if success, pointer to #Error_Info if failure
 */
Error_Info *
ddc_save_current_settings(
      Display_Handle * dh)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
          "Invoking DDC Save Current Settings command. dh=%s", dh_repr(dh));

   Error_Info * ddc_excp = NULL;
   if (dh->dref->io_path.io_mode == DDCA_IO_USB) {
      // command line parser should block this case
      PROGRAM_LOGIC_ERROR("MCCS over USB does not have Save Current Settings command");
      ddc_excp = ERRINFO_NEW(DDCRC_UNIMPLEMENTED,
                              "MCCS over USB does not have Save Current Settings command" );
   }
   else {
      DDC_Packet * request_packet_ptr =
         create_ddc_save_settings_request_packet("save_current_settings:request packet");
      // DBGMSG("create_ddc_save_settings_request_packet returned packet_ptr=%p", request_packet_ptr);
      // dump_packet(request_packet_ptr);

      ddc_excp = ddc_write_only_with_retry(dh, request_packet_ptr);

      free_ddc_packet(request_packet_ptr);
   }

   DBGTRC_RET_ERRINFO(debug, TRACE_GROUP, ddc_excp, "");
   return ddc_excp;
}


void
init_ddc_save_current_settings() {
   RTTI_ADD_FUNC(ddc_save_current_settings);
}

