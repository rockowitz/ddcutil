/** @file dw_services.c
 *
 * dw layer initialization and configuration
 */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <stdio.h>

#include "util/report_util.h"
/** \endcond */

#include "base/base_services.h"
#include "base/display_lock.h"
#include "base/display_retry_data.h"
#include "base/dsa2.h"
#include "base/feature_metadata.h"
#include "base/parms.h"
#include "base/per_thread_data.h"
#include "base/rtti.h"
#include "base/sleep.h"
#include "base/tuned_sleep.h"

#include "vcp/parse_capabilities.h"
#include "vcp/persistent_capabilities.h"
#include "vcp/vcp_feature_codes.h"

#include "dynvcp/vcp_feature_set.h"
#include "dynvcp/dyn_feature_codes.h"
#include "dynvcp/dyn_feature_set.h"
#include "dynvcp/dyn_feature_files.h"
#include "dynvcp/dyn_parsed_capabilities.h"

#include "sysfs/sysfs_services.h"

#include "i2c/i2c_services.h"

#ifdef ENABLE_USB
#include "usb/usb_services.h"
#endif

#include "ddc/ddc_common_init.h"
#include "ddc/ddc_display_selection.h"
#include "ddc/ddc_display_ref_reports.h"
#include "ddc/ddc_displays.h"
#include "ddc/ddc_dumpload.h"
#include "ddc/ddc_initial_checks.h"
#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_output.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_phantom_displays.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_serialize.h"
#include "ddc/ddc_save_current_settings.h"
#include "ddc/ddc_try_data.h"
#include "ddc/ddc_vcp.h"
#include "ddc/ddc_vcp_version.h"

#include "dw/dw_status_events.h"
#include "dw/dw_dref.h"
#include "dw/dw_xevent.h"
#include "dw/dw_udev.h"
#include "dw/dw_poll.h"
#include "dw/dw_main.h"
#include "dw/dw_common.h"

#include "dw_services.h"


/** Initialize dw directory
 */
void init_dw_services() {
   bool debug = false;
   DBGMSF(debug, "Starting");

     init_dw_dref();
   init_dw_xevent();
   init_dw_udev();
   init_dw_poll();
   init_dw_common();
   init_dw_main();

   if (debug)
      dbgrpt_rtti_func_name_table(1, /* show_internal*/ true);
   DBGMSF(debug, "Done");
}


void terminate_dw_services() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_CONN, "");

   DBGTRC_DONE(debug, DDCA_TRC_CONN, "");
}

