/** @file dsa1.c
 *
 *  Experimental dynamic sleep adjustment
 */

// Copyright (C) 2020-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


/** \cond */
#include <assert.h>
#include <base/dsa1.h>
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
#include "base/displays.h"
#include "base/sleep.h"
#include "base/parms.h"
#include "base/ddc_errno.h"
#include "base/linux_errno.h"
#include "base/rtti.h"
#include "base/per_display_data.h"

#include "base/dsa1.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_SLEEP;

const bool Default_DSA1_Enabled = DEFAULT_ENABLE_DSA1;
bool  dsa1_enabled        = Default_DSA1_Enabled;


DSA1_Data * new_dsa1_data(int busno) {
   DSA1_Data * dsa1 = calloc(1,sizeof(DSA1_Data));
   dsa1->busno = busno;
   return dsa1;
}


DSA1_Data * dsa1_data_from_dh(Display_Handle * dh) {
   Per_Display_Data * pdd = (Per_Display_Data*) dh->dref->pdd;
   DSA1_Data * dsa1 = (DSA1_Data*) pdd->dsa1_data;
   return dsa1;
}


// *** TO REVIEW: ***
void dsa1_reset_data(DSA1_Data * data) {
   pdd_cross_display_operation_block(__func__);
   data->cur_sleep_adjustment_factor = 1.0;
   data->adjusted_sleep_multiplier = 1.0;
   data->total_ok_status_count = 0;
   data->total_error_status_count = 0;
   data->total_other_status_ct = 0;
   data->total_adjustment_checks = 0;
   data->total_adjustment_ct = 0;
}

double
dsa1_get_adjusted_sleep_multiplier(DSA1_Data * data) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "data=%p, busno=%d", data, data->busno); ;

   double result = data->adjusted_sleep_multiplier;

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning: %7.2f", result);
   return result;
}



static int dsa_required_status_sample_size = 3;

bool dsa1_error_rate_is_high(DSA1_Data * dsa1) {
   bool debug = false;
   bool result = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "current_ok_status_count=%d, current_error_status_count=%d",
         dsa1->cur_ok_status_count, dsa1->cur_error_status_count);
   // compare_ptd_pdd_dsa_data(__func__,ptd,pdd);

   double dsa_error_rate_threshold = .1;

   double error_rate = 0.0;    // outside of loop for final debug message

   int current_total_count = dsa1->cur_ok_status_count + dsa1->cur_error_status_count;

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

      error_rate = (1.0 * dsa1->cur_error_status_count) / (current_total_count);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                    "ok_status_count=%d, error_status_count=%d,"
                    " error_rate = %7.2f, error_rate_threshold= %7.2f",
                    dsa1->cur_ok_status_count, dsa1->cur_error_status_count,
                    error_rate, dsa_error_rate_threshold);
      result = (error_rate > dsa_error_rate_threshold);
   }

   DBGTRC_RET_BOOL(debug, TRACE_GROUP, result, "total_count=%d, error_rate=%4.2f",
                          current_total_count, error_rate);
   return result;
}


// calculate the multiplier to be applied to the current multiplier
double dsa1_calc_readjustment_factor( double current_multiplier) {
   bool debug = false;

   if (current_multiplier == 0.0f)
      current_multiplier = .01;

   double result = 1.0;
   if (current_multiplier <= .2)
      result = 4.0;
   else if (current_multiplier <= .6 )
      result = 3.0;
   else if (current_multiplier <= 1.0)
      result = 2.0;
   else if (current_multiplier <= 3.0)
      result = 1.5;
   else
      result = 1.2;

   DBGMSF(debug, "current_multiplier = %3.2f, returning %3.2f",
                 current_multiplier, result);
   return result;
}


void dsa1_update_adjustment_factor_by_pdd(Per_Display_Data * pdd) {
   DSA1_Data * dsa1 = (DSA1_Data*) pdd->dsa1_data;

   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "pdd=%p", pdd);

#ifdef RETAINED_FOR_REFERENCE
   if (!dsa1) {
      double result = pdd->user_sleep_multiplier;  // --sleep-multiplier specified by user
      DBGTRC_DONE(debug, TRACE_GROUP, "dsa disabled, returning %7.1f", result);
      return result;
   }
#endif

   DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                   "dsa1 calls_since_last_check = %d, adjustment_check_interval = %d",
                   dsa1->calls_since_last_check, dsa1->adjustment_check_interval);

   bool sleep_adjustment_changed = false;
   double denominator = pdd->user_sleep_multiplier;  // --sleep-multiplier set by user
   if (denominator == 0) {
      DBGMSG("denominator == 0");
      denominator = .01;
   }
   double max_adjustment_factor = 4.0f;
   if (dsa1->calls_since_last_check > dsa1->adjustment_check_interval) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Performing check");
      dsa1->calls_since_last_check = 0;
      dsa1->total_adjustment_checks++;
      dsa1->calls_since_last_check = 0;
      dsa1->total_adjustment_checks++;

      int current_total_count = dsa1->cur_ok_status_count + dsa1->cur_error_status_count;
      if (current_total_count >= dsa_required_status_sample_size) {
         if (dsa1_error_rate_is_high(dsa1)) {
            if (dsa1->cur_sleep_adjustment_factor <= max_adjustment_factor) {
               // double d = 2 * dsa1->current_sleep_adjustment_factor;
               double d = dsa1_calc_readjustment_factor(pdd->adjusted_sleep_multiplier);
               if (d <= max_adjustment_factor) {
                     pdd->adjusted_sleep_multiplier = d;
               }
               else {
                  pdd->adjusted_sleep_multiplier = max_adjustment_factor;
               }
               sleep_adjustment_changed = true;
               pdd->most_recent_adjusted_sleep_multiplier = pdd->adjusted_sleep_multiplier;
               dsa1->total_adjustment_ct++;
               dsa1->total_adjustment_ct++;
               // dsa1->adjustment_check_interval = 2 * dsa1->adjustment_check_interval;
            }
            DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                  "sleep_adjustment_changed = %s, "
                   "New sleep_adjustment_factor %5.2f",
                   sbool(sleep_adjustment_changed),
                   pdd->adjusted_sleep_multiplier);
         }

         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "sleep_adjustment_changed=%s", sbool(sleep_adjustment_changed));
         if (sleep_adjustment_changed) {
            // reset the current status counts
            dsa1->cur_ok_status_count = 0;
            dsa1->cur_error_status_count = 0;
         }
      }
      else
         DBGTRC_NOPREFIX(debug, TRACE_GROUP, "Inadequate sample size");
   }
   else {
      dsa1->calls_since_last_check++;
      dsa1->calls_since_last_check++;
   }



   DBGTRC_DONE(debug, TRACE_GROUP,
           "current_ok_status_count=%d, current_error_status_count=%d, returning %5.2f",
           dsa1->cur_ok_status_count,
           dsa1->cur_error_status_count,
           dsa1->cur_sleep_adjustment_factor);
}


void
dsa1_note_retryable_failure_by_pdd(Per_Display_Data * pdd, int remaining_tries) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dpath=%s, remaining_tries=%d, dsa1_enabled=%s",
         dpath_repr_t(&pdd->dpath), remaining_tries, sbool(dsa1_enabled));
   assert(pdd);
   DSA1_Data * dsa1 = (DSA1_Data*) pdd->dsa1_data;

   dsa1->cur_error_status_count++;
   dsa1->total_error_status_count++;
   dsa1_update_adjustment_factor_by_pdd(pdd);

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


void dsa1_record_final_by_pdd(Per_Display_Data * pdd, DDCA_Status ddcrc, int retries) {
   bool debug = false;
   DBGMSF(debug, "ddcrc=%s", psc_desc(ddcrc));
   // Per_Display_Data * pdd = pdd_get_per_display_data(dpath, false);
   assert(pdd);
   DSA1_Data * dsa1 = (DSA1_Data*) pdd->dsa1_data;

   if (ddcrc == DDCRC_OK) {
      dsa1->cur_ok_status_count++;
      dsa1->total_ok_status_count++;
   }
   else if (ddcrc == DDCRC_DDC_DATA ||
            ddcrc == DDCRC_READ_ALL_ZERO ||
            ddcrc == -ENXIO  || // this is problematic - could indicate data error or actual response
            ddcrc == -EIO    ||   // but that's ok - be pessimistic re error rates
            ddcrc == DDCRC_NULL_RESPONSE  // can be either a valid "No Value" response, or indicate a display error
           )
   {
      dsa1->cur_error_status_count++;
      dsa1->total_error_status_count++;
      dsa1_update_adjustment_factor_by_pdd(pdd);
   }
   else {
      DBGMSF(debug, "other status code: %s", psc_desc(ddcrc));
      dsa1->total_other_status_ct++;
   }
   DBGMSF(debug, "Done. current_ok_status_count=%d, current_error_status_count=%d",
                 dsa1->cur_ok_status_count, dsa1->cur_error_status_count);
}


void   dsa1_report(DSA1_Data * data, int depth) {
   int d1 = depth+1;
   if (data) {
   // if (data->dynamic_sleep_enabled)  {
      rpt_label(depth, "Dynamic sleep adjustment algorithm 1:");
      rpt_vstring(d1, "Total successful reads:           %5d",   data->total_ok_status_count);
      rpt_vstring(d1, "Total reads with DDC error:       %5d",   data->total_error_status_count);
      rpt_vstring(d1, "Total ignored status codes:       %5d",   data->total_other_status_ct);

      rpt_vstring(d1, "Total adjustment checks:          %5d",   data->total_adjustment_checks);
      rpt_vstring(d1, "Number of adjustments:            %5d",   data->total_adjustment_ct);
      rpt_vstring(d1, "cur_sleep_adjustmet_factor    : %3.2f",   data->cur_sleep_adjustment_factor);
   }
   else
      rpt_label(depth, "Dynamic sleep_adjustment algorithm 1: disabled");
}



void init_dsa1() {
   RTTI_ADD_FUNC(dsa1_calc_readjustment_factor);
   RTTI_ADD_FUNC(dsa1_error_rate_is_high);
}
