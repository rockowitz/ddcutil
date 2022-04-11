/** @file dynamic_sleep.c
 *
 *  Experimental dynamic sleep adjustment
 */

// Copyright (C) 2020-2022 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include "base/rtti.h"
#include "base/thread_sleep_data.h"

#include "base/dynamic_sleep.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_NONE;


void dsa_record_ddcrw_status_code(int rc) {
   bool debug = false;
   DBGMSF(debug, "rc=%s", psc_desc(rc));
   Per_Thread_Data * tsd = tsd_get_thread_sleep_data();

   if (rc == DDCRC_OK) {
      tsd->cur_ok_status_count++;
      tsd->total_ok_status_count++;
   }
   else if (rc == DDCRC_DDC_DATA ||
            rc == DDCRC_READ_ALL_ZERO ||
            rc == -ENXIO  || // this is problematic - could indicate data error or actual response
            rc == -EIO    ||   // but that's ok - be pessimistic re error rates
            rc == DDCRC_NULL_RESPONSE  // can be either a valid "No Value" response, or indicate a display error
           )
   {
      tsd->cur_error_status_count++;
      tsd->total_error_status_count++;
   }
   else {
      DBGMSF(debug, "other status code: %s", psc_desc(rc));
      tsd->total_other_status_ct++;
   }
   DBGMSF(debug, "Done. current_ok_status_count=%d, current_error_status_count=%d",
                 tsd->cur_ok_status_count, tsd->cur_error_status_count);
}


static void dsa_reset_cur_status_counts() {
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Executing");
   Per_Thread_Data * data = tsd_get_thread_sleep_data();

   data->cur_ok_status_count = 0;
   data->cur_error_status_count = 0;
}


static int dsa_required_status_sample_size = 3;

bool dsa_error_rate_is_high(Per_Thread_Data * tsd) {
   assert(tsd);
   bool debug = false;
   bool result = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "current_ok_status_count=%d, current_error_status_count=%d",
         tsd->cur_ok_status_count, tsd->cur_error_status_count);

   double dsa_error_rate_threshold = .1;

   double error_rate = 0.0;    // outside of loop for final debug message

   int current_total_count = tsd->cur_ok_status_count + tsd->cur_error_status_count;

   if ( (current_total_count) >= dsa_required_status_sample_size) {
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

      error_rate = (1.0 * tsd->cur_error_status_count) / (current_total_count);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                    "ok_status_count=%d, error_status_count=%d,"
                    " error_rate = %7.2f, error_rate_threshold= %7.2f",
                    tsd->cur_ok_status_count, tsd->cur_error_status_count,
                    error_rate, dsa_error_rate_threshold);
      result = (error_rate > dsa_error_rate_threshold);
   }
   // DBGMSF(debug, "%s", sbool(result));
   DBGTRC_DONE(debug, TRACE_GROUP, "total_count=%d, error_rate=%4.2f, returning %s",
                 current_total_count, error_rate, sbool(result));
   return result;
}


int dsa_calc_sleep_time(int cur_sleep_time_millis, int spec_sleep_time_millis) {
   double current_sleep_time = cur_sleep_time_millis;
   double spec_sleep_time = spec_sleep_time_millis;
   bool debug = false;
   int result;
   if (current_sleep_time <= .2 * spec_sleep_time)
      result = 4.0 * current_sleep_time;
   else if (current_sleep_time <= .6 * spec_sleep_time)
      result = 3.0 * current_sleep_time;
   else if (current_sleep_time <= 1.0 * spec_sleep_time)
      result = 2.0 * current_sleep_time;
   else if (current_sleep_time <= 3.0 * spec_sleep_time)
      result = 1.5 * current_sleep_time;
   else
      result = spec_sleep_time;

   DBGMSF(debug, "cur_sleep_time_millis = %d, returning %d",
                 cur_sleep_time_millis, result);
   return result;
}


double dsa_calc_adjustment_factor(
      int spec_sleep_time_millis,
      double multiplier_factor,
      double cur_factor)
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP,
             "spec_sleep_time_millis=%d, multiplier_factor=%4.1f, cur_factor=%4.1f",
             spec_sleep_time_millis, multiplier_factor, cur_factor);
   int cur_sleep_time_millis = spec_sleep_time_millis * multiplier_factor * cur_factor;
   int new_sleep_time_millis = dsa_calc_sleep_time(cur_sleep_time_millis, spec_sleep_time_millis);
   double new_factor = new_sleep_time_millis / (spec_sleep_time_millis * multiplier_factor);
   DBGTRC_DONE(debug, TRACE_GROUP, "Returning %4.1f", new_factor);
   return new_factor;
}


double dsa_update_adjustment_factor(Display_Handle * dh, int spec_sleep_time_millis) {
   bool debug = false;
   Per_Thread_Data * tsd = tsd_get_thread_sleep_data();
   DBGTRC_STARTING(debug, TRACE_GROUP, "dh=%s, dynamic_sleep_enabled for current thread = %s",
                            dh_repr_t(dh),              sbool(tsd->dynamic_sleep_enabled));
   if (!tsd->dynamic_sleep_enabled) {
      int result = tsd->sleep_multiplier_factor;
      DBGTRC_DONE(debug, TRACE_GROUP, "dsa disabled, returning %7.1f", result);
      return result;
   }

   if (dh != tsd->cur_dh) {
      tsd->cur_dh = dh;
      dsa_reset_cur_status_counts();
      tsd->cur_sleep_adjustment_factor  = 1.0;
      DBGTRC_DONE(debug, TRACE_GROUP, "dh changed, returning %4.2f" ,
                                      tsd->cur_sleep_adjustment_factor);
      return tsd->cur_sleep_adjustment_factor;
   }

   DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                   "calls_since_last_check = %d, adjustment_check_interval = %d",
                   tsd->calls_since_last_check, tsd->adjustment_check_interval);
   bool sleep_adjustment_changed = false;
   double max_factor = (spec_sleep_time_millis/tsd->sleep_multiplier_factor) * 3.0f;
   if (tsd->calls_since_last_check > tsd->adjustment_check_interval) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Performing check");
      tsd->calls_since_last_check = 0;
      tsd->total_adjustment_checks++;

      int current_total_count = tsd->cur_ok_status_count + tsd->cur_error_status_count;

      if ( current_total_count >= dsa_required_status_sample_size) {
         if (dsa_error_rate_is_high(tsd)) {
            if (tsd->cur_sleep_adjustment_factor < max_factor) {
               // double d = 2 * tsd->current_sleep_adjustment_factor;
               double d = dsa_calc_adjustment_factor(
                     spec_sleep_time_millis,
                     tsd->sleep_multiplier_factor,
                     tsd->cur_sleep_adjustment_factor);
               if (d <= max_factor) {
                     tsd->cur_sleep_adjustment_factor = d;
               }
               else {
                  tsd->cur_sleep_adjustment_factor = max_factor;
               }
               sleep_adjustment_changed = true;
               tsd->total_adjustment_ct++;
               // tsd->adjustment_check_interval = 2 * tsd->adjustment_check_interval;
            }
            DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                  "sleep_adjustment_changed = %s, "
                   "New sleep_adjustment_factor %5.2f",
                   sbool(sleep_adjustment_changed),
                   tsd->cur_sleep_adjustment_factor);
         }

         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "sleep_adjustment_changed=%s", sbool(sleep_adjustment_changed));
         if (sleep_adjustment_changed)
            dsa_reset_cur_status_counts();
      }
      else
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Inadequate sample size");
   }
   else
      tsd->calls_since_last_check++;

   DBGTRC_DONE(debug, TRACE_GROUP,
           "current_ok_status_count=%d, current_error_status_count=%d, returning %5.2f",
           tsd->cur_ok_status_count,
           tsd->cur_error_status_count,
           tsd->cur_sleep_adjustment_factor);
   return tsd->cur_sleep_adjustment_factor;
}


void init_dynamic_sleep() {
   RTTI_ADD_FUNC(dsa_calc_adjustment_factor);
   RTTI_ADD_FUNC(dsa_calc_sleep_time);
   RTTI_ADD_FUNC(dsa_update_adjustment_factor);
   RTTI_ADD_FUNC(dsa_error_rate_is_high);
}
