/* ddc_services.c
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \file
 *  DDC initialization and configuration, statistics management
 */

/** \cond */
#include <stdio.h>
/** \endcond */

#include "util/report_util.h"

#include "base/adl_errors.h"
#include "base/base_init.h"
#include "base/parms.h"
#include "base/sleep.h"

#include "vcp/vcp_feature_codes.h"

#include "i2c/i2c_do_io.h"

#include "adl/adl_shim.h"

#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"

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
      report_sleep_strategy_stats(depth);
      rpt_nl();
      report_io_call_stats(depth);
      rpt_nl();
      report_sleep_stats(depth);
   }
   if (stats & (DDCA_STATS_ELAPSED | DDCA_STATS_CALLS)) {
      rpt_nl();
      report_elapsed_stats(depth);
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
               MAX_WRITE_ONLY_EXCHANGE_TRIES);
   rpt_vstring(depth, "Write read exchange tries:       %8d %8d",
               ddc_get_max_write_read_exchange_tries(),
               MAX_WRITE_READ_EXCHANGE_TRIES);
   rpt_vstring(depth, "Multi-part read exchange tries:  %8d %8d",
               ddc_get_max_multi_part_read_tries(),
               MAX_MULTI_EXCHANGE_TRIES);
   rpt_vstring(depth, "Multi-part write exchange tries: %8d %8d",
               ddc_get_max_multi_part_write_tries(),
               MAX_MULTI_EXCHANGE_TRIES);
}


/** Master initialization function for DDC services
 */
void init_ddc_services() {
   bool debug = false;
   DBGMSF0(debug, "Executing");

   // i2c:
   i2c_set_io_strategy(DEFAULT_I2C_IO_STRATEGY);

   // adl:
   init_adl_errors();
   adl_debug = debug;      // turn on adl initialization tracing
   adlshim_initialize();

   // ddc:
   ddc_reset_ddc_stats();
   init_vcp_feature_codes();
}
