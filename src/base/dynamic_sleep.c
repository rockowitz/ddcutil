/** @file dynamic_sleep.c
 *
 *  Experimental dynamic sleep adjustment
 */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "ddcutil_status_codes.h"

#include "util/glib_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/sleep.h"
#include "base/parms.h"
#include "base/ddc_errno.h"
#include "base/linux_errno.h"
#include "base/thread_sleep_data.h"

#include "base/dynamic_sleep.h"

static bool global_dynamic_sleep_enabled = false;  // across all threads

void dsa_enable(bool enabled) {
   bool debug = false;
   DBGMSF(debug, "Executing.  enabled = %s", sbool(enabled));
   tsd_enable_dynamic_sleep(enabled);
}

// Enable or disable dynamic sleep for all current threads and new threads
void global_dsa_enable(bool enabled) {
   bool debug = false;
   DBGMSF(debug, "Executing.  enabled = %s", sbool(enabled));
   global_dynamic_sleep_enabled = enabled;
   tsd_enable_dynamic_sleep_all(enabled) ;
}

// Is dynamic sleep enabled on the current thread?
bool dsa_is_enabled() {
   Thread_Sleep_Data * tsd = get_thread_sleep_data(true);
   return tsd->dynamic_sleep_enabled;
}


void dsa_record_ddcrw_status_code(int rc) {
   bool debug = false;
   DBGMSF(debug, "rc=%s", psc_desc(rc));
   Thread_Sleep_Data * tsd = get_thread_sleep_data(true);

   if (rc == DDCRC_OK) {
      tsd->current_ok_status_count++;
      tsd->total_ok_status_count++;
   }
   else if (rc == DDCRC_DDC_DATA ||
            rc == DDCRC_READ_ALL_ZERO ||
            rc == -ENXIO  || // this is problematic - could indicate data error or actual response
            rc == -EIO    ||   // but that's ok - be pessimistic re error rates
            rc == DDCRC_NULL_RESPONSE  // can be either a valid "No Value" response, or indicate a display error
           )
   {
      tsd->current_error_status_count++;
      tsd->total_error_status_count++;
   }
   else {
      DBGMSF(true, "other status code: %s", psc_desc(rc));
      tsd->total_other_status_ct++;
   }
   DBGMSF(debug, "Done. current_ok_status_count=%d, current_error_status_count=%d",
                 tsd->current_ok_status_count, tsd->current_error_status_count);
}


void dsa_reset_counts() {
   bool debug = false;
   DBGMSF(debug, "Executing");
   Thread_Sleep_Data * data = get_thread_sleep_data(true);

   data->current_ok_status_count = 0;
   data->current_error_status_count = 0;
}


bool error_rate_is_high(Thread_Sleep_Data * tsd) {
   bool debug = false;
   bool result = false;
   DBGMSF(debug, "Starting");
   assert(tsd);

   double dsa_error_rate_threshold = .1;
   int    dsa_required_status_sample_size = 3;

   double error_rate = 0.0;    // outside of loop for final debug message

   int current_total_count = tsd->current_ok_status_count + tsd->current_error_status_count;

   if ( (current_total_count) > dsa_required_status_sample_size) {
      if (current_total_count <= 4) {
         dsa_error_rate_threshold = .5;
         // adjustment_check_interval = 3;
      }
      else if (current_total_count <= 10) {
         dsa_error_rate_threshold = .3;
         // adjustment_check_interval = 4;
      }
      else {
         dsa_error_rate_threshold = .1;
         // adjustment_check_interval = 5;
      }

      error_rate = (1.0 * tsd->current_error_status_count) / (current_total_count);
      DBGMSF(debug, "ok_status_count=%d, error_status_count=%d,"
                    " error_rate = %7.2f, error_rate_threshold= %7.2f",
                    tsd->current_ok_status_count, tsd->current_error_status_count,
                    error_rate, dsa_error_rate_threshold);
      result = (error_rate > dsa_error_rate_threshold);
   }
   // DBGMSF(debug, "%s", sbool(result));
   DBGMSF(debug, "total_count=%d, error_rate=%4.2f, returning %s",
                 current_total_count, error_rate, sbool(result));
   return result;
}


// This function is a swamp - refactor

double dsa_get_sleep_adjustment() {
   bool debug = false;
   Thread_Sleep_Data * tsd = get_thread_sleep_data(true);
   DBGMSF(debug, "global_dynamic_sleep_enabled for current thread = %s", sbool(tsd->dynamic_sleep_enabled));
   if (!tsd->dynamic_sleep_enabled) {
      double result = 1.0;
      DBGMSF(debug, "Returning %3.1f" ,result);
      return result;
   }

   // double dsa_increment = .5;                      // a constant, for now
   // int adjustment_check_interval = 2;    //  move to Thread_Sleep_Data  ??

   tsd->calls_since_last_check++;

   if (tsd->calls_since_last_check > tsd->adjustment_check_interval) {
      DBGMSF(debug, "calls_since_last_check = %d, adjustment_check_interval = %d, performing check",
                    tsd->calls_since_last_check, tsd->adjustment_check_interval);
      tsd->calls_since_last_check = 0;
      tsd->total_adjustment_checks++;

      if (error_rate_is_high(tsd)) {
         double max_sleep_adjustment_factor = 3.0;
         if (max_sleep_adjustment_factor < 2 * tsd->sleep_multiplier_factor)
            max_sleep_adjustment_factor = 2 * tsd->sleep_multiplier_factor;

         double next_sleep_adjustment_factor =
             tsd->current_sleep_adjustment_factor + tsd->thread_adjustment_increment;

         if (next_sleep_adjustment_factor <= max_sleep_adjustment_factor) {
            tsd->adjustment_ct++;
            tsd->current_sleep_adjustment_factor = next_sleep_adjustment_factor;
            tsd->thread_adjustment_increment = 2.0 * tsd->thread_adjustment_increment;
            tsd->adjustment_check_interval = 2 * tsd->adjustment_check_interval;
            DBGMSF(debug, "Increasing sleep_adjustment_factor to %5.2f,"
                          " thread_adjustment_increment to %5.2f",
                          tsd->current_sleep_adjustment_factor,
                          tsd->thread_adjustment_increment);
         }
         else {
            tsd->non_adjustment_ct++;
            tsd->adjustment_check_interval = 999;   // no more checks

            DBGMSF(debug, "Max sleep adjustment factor reached.  Returning %5.2f",
                          tsd->current_sleep_adjustment_factor);
         }
         dsa_reset_counts();
      }
   }
   float result = tsd->current_sleep_adjustment_factor;

   DBGMSF(debug, "current_ok_status_count=%d, current_error_status_count=%d, returning %5.2f",
           tsd->current_ok_status_count, tsd->current_error_status_count, result);
   return result;
}

