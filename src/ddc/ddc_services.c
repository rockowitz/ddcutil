/** @file ddc_services.c
 *
 * ddc layer initialization and configuration, statistics management
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <i2c/i2c_strategy_dispatcher.h>
#include <stdio.h>
/** \endcond */

#include "util/report_util.h"

#include "base/adl_errors.h"
#include "base/base_init.h"
// #include "base/dynamic_sleep.h"
#include "base/feature_metadata.h"
#include "base/parms.h"
#include "base/sleep.h"
#include "base/tuned_sleep.h"
#include "base/thread_retry_data.h"
#include "base/thread_sleep_data.h"

#include "vcp/vcp_feature_codes.h"

#include "dynvcp/dyn_feature_codes.h"

#include "i2c/i2c_bus_core.h"
#include "adl/adl_shim.h"

#include "usb/usb_displays.h"

#include "ddc/ddc_display_lock.h"
#include "ddc/ddc_displays.h"
#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_try_stats.h"
#include "ddc/ddc_vcp.h"

#include "ddc/ddc_services.h"


//
// Statistics
//

/** Resets all DDC level statistics */
void ddc_reset_ddc_stats() {
   ddc_reset_write_only_stats();
   ddc_reset_write_read_stats();
   ddc_reset_multi_part_read_stats();
   ddc_reset_multi_part_write_stats();
}


/** Reports all DDC level statistics
 * \param depth logical indentation depth
 */
void ddc_report_ddc_stats(int depth) {
   rpt_nl();
   // retry related stats
   ddc_report_max_tries(0);
   ddc_report_write_only_stats(0);
   ddc_report_write_read_stats(0);
   ddc_report_multi_part_read_stats(0);
   ddc_report_multi_part_write_stats(0);
}


/** Resets all statistics */
void ddc_reset_stats_main() {
   ddc_reset_ddc_stats();
   reset_execution_stats();
}


/** Master function for reporting statistics.
 *
 * \param stats bitflags indicating which statistics to report
 * \param depth logical indentation depth
 */
void ddc_report_stats_main(DDCA_Stats_Type stats, int depth) {
   if (stats & DDCA_STATS_TRIES) {
      ddc_report_ddc_stats(depth);
   }
   if (stats & DDCA_STATS_ERRORS) {
      rpt_nl(); ;
      show_all_status_counts();   // error code counts
   }
   if (stats & DDCA_STATS_CALLS) {
      rpt_nl();
      report_execution_stats(depth);
      rpt_nl();
#ifdef OLD
      if (dsa_is_enabled()) {
         // for now just report current thread
         dsa_report_stats(depth);
         rpt_nl();
      }
#endif
      report_all_thread_sleep_data(depth);
      // rpt_nl();
      report_io_call_stats(depth);
      rpt_nl();
      report_sleep_stats(depth);
   }
   if (stats & ( DDCA_STATS_CALLS)) {
      rpt_nl();
      report_elapsed_stats(depth);
      rpt_nl();
      // seeing the maxtries settings for each
      // report_all_thread_retry_data(depth);
   }
   if (stats & (DDCA_STATS_ELAPSED)) {
      rpt_nl();
      report_elapsed_summary(depth);
   }
}


/** Reports the current max try settings.
 *
 *  \param depth logical indentation depth
 */
void ddc_report_max_tries(int depth) {
   rpt_vstring(depth, "Maximum Try Settings:");
   rpt_vstring(depth, "Operation Type                    Current  Default");
   rpt_vstring(depth, "Write only exchange tries:       %8d %8d",
               ddc_get_max_write_only_exchange_tries(),
               INITIAL_MAX_WRITE_ONLY_EXCHANGE_TRIES);
   rpt_vstring(depth, "Write read exchange tries:       %8d %8d",
               ddc_get_max_write_read_exchange_tries(),
               INITIAL_MAX_WRITE_READ_EXCHANGE_TRIES);
   rpt_vstring(depth, "Multi-part read exchange tries:  %8d %8d",
               ddc_get_max_multi_part_read_tries(),
               INITIAL_MAX_MULTI_EXCHANGE_TRIES);
   rpt_vstring(depth, "Multi-part write exchange tries: %8d %8d",
               ddc_get_max_multi_part_write_tries(),
               INITIAL_MAX_MULTI_EXCHANGE_TRIES);
}


/** Master initialization function for DDC services
 */
void init_ddc_services() {
   bool debug = false;
   DBGMSF(debug, "Executing");

   // i2c:
   i2c_set_io_strategy(DEFAULT_I2C_IO_STRATEGY);

   // adl:
   init_adl_errors();
   adl_debug = debug;      // turn on adl initialization tracing
   adlshim_initialize();

   // i2c
   init_i2c_bus_core();

   // usb
#ifdef USE_USB
   init_usb_displays();
#endif

   // ddc:
   init_vcp_feature_codes();
   init_dyn_feature_codes();    // must come after init_vcp_feature_codes()
   init_ddc_display_lock();
   init_ddc_displays();
   init_ddc_packet_io();
   init_ddc_multi_part_io();
   init_ddc_try_data();
   init_ddc_vcp();

   // dbgrpt_func_name_table(1);
}
