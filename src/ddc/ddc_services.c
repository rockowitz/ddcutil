/** @file ddc_services.c
 *
 * ddc layer initialization and configuration, statistics management
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <stdio.h>

#include "util/report_util.h"
/** \endcond */

#include "base/base_services.h"
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

#include "dynvcp/dyn_feature_codes.h"
#include "dynvcp/dyn_feature_files.h"

#include "i2c/i2c_services.h"

#ifdef ENABLE_USB
#include "usb/usb_services.h"
#endif

#include "ddc/ddc_common_init.h"
#include "ddc/ddc_display_selection.h"
#include "i2c/i2c_display_lock.h"
#include "ddc/ddc_display_ref_reports.h"
#include "ddc/ddc_displays.h"
#include "ddc/ddc_dumpload.h"
#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_output.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_read_capabilities.h"
#include "ddc/ddc_serialize.h"
#include "ddc/ddc_status_events.h"
#include "ddc/ddc_try_data.h"
#include "ddc/ddc_vcp.h"
#ifdef BUILD_SHARED_LIB
#include "ddc/ddc_watch_displays.h"
#endif

#include "ddc/ddc_services.h"


//
// Statistics
//

// /** Resets all DDC level statistics */
// void ddc_reset_ddc_stats() {
//    try_data_reset2_all();
// }


/** Resets all statistics */
void ddc_reset_stats_main() {
   // ddc_reset_ddc_stats();
   try_data_reset2_all();
   reset_execution_stats();
   ptd_profile_reset_all_stats();
}


/** Master function for reporting statistics.
 *
 * \param stats                 bitflags indicating which statistics to report
 * \param show_per_display      include per display execution stats
 * \param include_dsa_internal  include internal dsa info in per display elapsed stats
 * \param depth logical         indentation depth
 */
void ddc_report_stats_main(DDCA_Stats_Type  stats,
                           bool             show_per_display_stats,
                           bool             include_dsa_internal,
                           bool             stats_to_syslog_only,
                           int depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_DDC, "stats: 0x%02x, show_per_thread_stats: %s, include_dsa_internal: %s",
         stats, sbool(show_per_display_stats), sbool(include_dsa_internal));

   if (stats_to_syslog_only) {
      start_capture(DDCA_CAPTURE_STDERR);
   }

   rpt_nl();
   rpt_label(depth, "EXECUTION STATISTICS");
   rpt_nl();

   if (stats & DDCA_STATS_TRIES) {
      ddc_report_ddc_stats(depth);
      rpt_nl();
   }

   if (stats & DDCA_STATS_ERRORS) {
      report_all_status_counts(depth);   // error code counts
      rpt_nl();
   }

   if (stats & DDCA_STATS_CALLS) {
      report_execution_stats(depth);
      rpt_nl();

#ifdef OLD
      if (dsa_is_enabled()) {
         // for now just report current thread
         dsa_report_stats(depth);
         rpt_nl();
      }
#endif

      report_io_call_stats(depth);
      rpt_nl();
      report_sleep_stats(depth);
      rpt_nl();
      report_elapsed_stats(depth);
      rpt_nl();
   }

   if (stats & (DDCA_STATS_ELAPSED)) {
      report_elapsed_summary(depth);
      rpt_nl();
   }

   if (show_per_display_stats) {
      rpt_label(depth, "PER-DISPLAY EXECUTION STATISTICS");
      rpt_nl();
       if (stats & DDCA_STATS_TRIES) {
           drd_report_all_display_retry_data(depth);
      }
      if (stats & DDCA_STATS_ERRORS) {
          pdd_report_all_per_display_error_counts(depth);
      }
      if (stats & DDCA_STATS_CALLS) {
         pdd_report_all_per_display_call_stats(depth);
      }
      if (stats & (DDCA_STATS_ELAPSED)) {
          // need a report_all_thread_elapsed_summary()
          pdd_report_all_per_display_elapsed_stats(include_dsa_internal, depth);
      }

      // Reports locks held by per_thread_data() to confirm that locking has
      // trivial affect on performance.
      //dbgrpt_per_thread_data_locks(depth+1);
   }

   if (ptd_api_profiling_enabled) {
      ptd_profile_report_all_threads(0);
      ptd_profile_report_stats_summary(0);
   }

   if (stats_to_syslog_only) {
      char * result = end_capture();
      Null_Terminated_String_Array lines = strsplit(result, "\n");
      free(result);
      int len = ntsa_length(lines);
      int ndx;
      for (ndx=0; ndx<len; ndx++) {
         syslog(LOG_INFO, "%s", lines[ndx]);
         // printf("%s\n", lines[ndx]);
      }
      ntsa_free(lines, true);
   }

   DBGTRC_DONE(debug, DDCA_TRC_DDC, "");
}


/** Master initialization function for DDC services
 */
void init_ddc_services() {
   bool debug = false;
   DBGMSF(debug, "Starting");

   // i2c:
   init_i2c_services();

   // usb
#ifdef ENABLE_USB
   init_usb_services();
#endif

   // vcp:
   init_persistent_capabilities();
   init_parse_capabilities();
   init_vcp_feature_codes();

   // dyn:
   init_dyn_feature_codes();    // must come after init_vcp_feature_codes()
   init_dyn_feature_files();

   // i2c:
   init_i2c_display_lock();

   // ddc:
   init_ddc_common_init();
   init_ddc_try_data();
   init_ddc_display_selection();
   init_ddc_display_ref_reports();
   init_ddc_displays();
   init_ddc_dumpload();
   init_ddc_output();
   init_ddc_packet_io();
   init_ddc_read_capabilities();
   init_ddc_serialize();
   init_ddc_status_events();
   init_ddc_multi_part_io();
   init_ddc_vcp();
// #ifdef BUILD_SHARED_LIB
   init_ddc_watch_displays();
// #endif

   RTTI_ADD_FUNC(ddc_report_stats_main);

   if (debug)
      dbgrpt_rtti_func_name_table(1, /* show_internal*/ true);
   DBGMSF(debug, "Done");
}


// also handles VCP
void terminate_ddc_services() {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_DDCIO, "");
   terminate_ddc_serialize();
   terminate_ddc_displays();  // must be called before terminate_ddc_packet_io()
   terminate_ddc_packet_io();
   terminate_i2c_display_lock();

   terminate_persistent_capabilities();
#ifdef ENABLE_USB
   terminate_usb_services();
#endif
   terminate_i2c_services();
   DBGTRC_DONE(debug, DDCA_TRC_DDCIO, "");
}

