// dynamic_sleep.c

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




static bool dynamic_sleep_adjustment_enabled = false;

void dsa_enable(bool enabled) {
   dynamic_sleep_adjustment_enabled = enabled;
}

bool dsa_is_enabled() {
   return dynamic_sleep_adjustment_enabled;
}

#ifdef OLD

typedef struct {
   int    ok_status_count;
   int    error_status_count;
   int    total_ok;
   int    total_error;
   int    other_status_ct;
   int    adjustment_ct;
   int    non_adjustment_ct;
   int    max_adjustment_ct;
   float  current_sleep_adjustment_factor;
   double sleep_multiplier_factor;   // as set by user
   bool   initialized;
}  Dynamic_Sleep_Adjustment_Stats;



void dbgrpt_dsa_stats(Dynamic_Sleep_Adjustment_Stats * stats, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Error_Stats", stats, depth);
   rpt_bool("initialized",       NULL, stats->initialized,        d1);
   rpt_int("ok_status_count",    NULL, stats->ok_status_count,    d1);
   rpt_int("error_status_count", NULL, stats->error_status_count, d1);
   rpt_int("other_status_ct",    NULL, stats->other_status_ct,    d1);
   rpt_int("total_ok",           NULL, stats->total_ok,           d1);
   rpt_int("total_error",        NULL, stats->total_error,        d1);
   rpt_int("adjustment_ct",      NULL, stats->adjustment_ct,      d1);
   rpt_int("max_adjustment_ct",    NULL, stats->max_adjustment_ct, d1);
   rpt_int("non_adjustment_ct",  NULL, stats->non_adjustment_ct,  d1);
   rpt_vstring(d1, "sleep-multiplier value:    %5.2f", stats->sleep_multiplier_factor);
   rpt_vstring(d1, "current_sleep_adjustment_factor:         %5.2f", stats->current_sleep_adjustment_factor);
}


// TODO: make per thread as in tuned_sleep.h

Dynamic_Sleep_Adjustment_Stats * dsa_get_stats_t() {
   static GPrivate  buf_key = G_PRIVATE_INIT(g_free);
   Dynamic_Sleep_Adjustment_Stats * stats = (Dynamic_Sleep_Adjustment_Stats *) get_thread_fixed_buffer(&buf_key, sizeof(Dynamic_Sleep_Adjustment_Stats));

   // DBGMSG("Current error stats: ");
   // dbgrpt_error_stats(stats, 2);

   if (!stats->initialized) {
      stats->current_sleep_adjustment_factor = 1.0;
      stats->initialized = true;
      stats->sleep_multiplier_factor = 1.0;    // default
   }

   return stats;
}


void dsa_set_sleep_multiplier_factor(double factor) {
   set_global_sleep_multiplier_factor(factor);  // new way

   bool debug = false;
   DBGMSF(debug, "factor=%5.2f", factor);
   Dynamic_Sleep_Adjustment_Stats * stats = dsa_get_stats_t();
   stats->sleep_multiplier_factor = factor;
}
#endif

void dsa_record_ddcrw_status_code(int rc) {
   bool debug = false;
   DBGMSF(debug, "rc=%s", psc_desc(rc));
   // Dynamic_Sleep_Adjustment_Stats * stats = dsa_get_stats_t();
   Thread_Sleep_Data * tsd = get_thread_sleep_data(true);

   if (rc == DDCRC_OK) {
      // stats->ok_status_count++;
      // stats->total_ok++;
      tsd->ok_status_count++;
      tsd->total_ok++;
   }
   else if (rc == DDCRC_DDC_DATA ||
            rc == DDCRC_READ_ALL_ZERO ||
            rc == -ENXIO  || // this is problematic - could indicate data error or actual response
            rc == -EIO    ||   // but that's ok - be pessimistic re error rates
            rc == DDCRC_NULL_RESPONSE  // can be either a valid "No Value" response, or indicate a display error
           )
   {
      // stats->error_status_count++;
      // stats->total_error++;

      tsd->error_status_count++;
      tsd->total_error++;
   }
   else {
      DBGMSF(true, "other status code: %s", psc_desc(rc));
      // stats->other_status_ct++;
      tsd->other_status_ct++;
   }
   DBGMSF(debug, "Done. ok_status_count=%d, error_status_count=%d",
                 tsd->ok_status_count, tsd->error_status_count);
}


#ifdef OLD
void dsa_reset_counts() {
   bool debug = false;
   DBGMSF(debug, "Executing");
   Dynamic_Sleep_Adjustment_Stats * stats = dsa_get_stats_t();

   stats->ok_status_count = 0;
   stats->error_status_count = 0;
}
#endif

void dsa_reset_counts() {
   bool debug = false;
   DBGMSF(debug, "Executing");
   Thread_Sleep_Data * stats = get_thread_sleep_data(true);

   stats->ok_status_count = 0;
   stats->error_status_count = 0;
}



double dsa_get_sleep_adjustment() {
   bool debug = false;
   DBGMSF(debug, "dynamic_sleep_adjustment_enabled = %s", sbool(dynamic_sleep_adjustment_enabled));
   if (!dynamic_sleep_adjustment_enabled) {
      double result = 1.0;
      DBGMSF(debug, "Returning %3.1f" ,result);
      return result;
   }

   double dsa_error_rate_threshold = .1;
   int    dsa_required_status_sample_size = 3;
   double dsa_increment = .5;                      // a constant, for now
   int adjustment_check_interval = 2;    //  move to Thread_Sleep_Data  ??

   Thread_Sleep_Data * tsd = get_thread_sleep_data(true);
   tsd->calls_since_last_check++;

   if (tsd->calls_since_last_check > adjustment_check_interval) {
      tsd->calls_since_last_check = 0;
      tsd->total_adjustment_checks++;

      int total_count = tsd->ok_status_count + tsd->error_status_count;

      if ( (total_count) > dsa_required_status_sample_size) {
         if (total_count <= 4) {
            dsa_error_rate_threshold = .5;
            // adjustment_check_interval = 3;
         }
         else if (total_count <= 10) {
            dsa_error_rate_threshold = .3;
            // adjustment_check_interval = 4;
         }
         else {
            dsa_error_rate_threshold = .1;
            // adjustment_check_interval = 5;
         }

         double error_rate = (1.0 * tsd->error_status_count) / (total_count);
         DBGMSF(debug, "ok_status_count=%d, error_status_count=%d, error_rate = %7.2f, error_rate_threshold= %7.2f",
               tsd->ok_status_count, tsd->error_status_count, error_rate, dsa_error_rate_threshold);
         if ( (1.0f * tsd->error_status_count) / (total_count) > dsa_error_rate_threshold ) {
            float next_sleep_adjustment_factor = tsd->current_sleep_adjustment_factor + dsa_increment;
            tsd->adjustment_ct++;
            double max_sleep_adjustment_factor = 3.0/tsd->sleep_multiplier_factor;
            if (next_sleep_adjustment_factor <= max_sleep_adjustment_factor) {

               tsd->current_sleep_adjustment_factor = next_sleep_adjustment_factor;
               DBGMSF(debug, "Increasing sleep_adjustment_factor to %f", tsd->current_sleep_adjustment_factor);
               dsa_reset_counts();
            }
            else {
               tsd->max_adjustment_ct++;
               DBGMSF(debug, "Max sleep adjustment factor reached.  Returning %9.1f", tsd->current_sleep_adjustment_factor);
            }
         }
         else {
            tsd->non_adjustment_ct++;
         }
      }
   }
   float result = tsd->current_sleep_adjustment_factor;

   DBGMSF(debug, "ok_status_count=%d, error_status_count=%d, returning %9.1f",
           tsd->ok_status_count, tsd->error_status_count, result);
   return result;
}

#ifdef OLD

void dsa_report_stats(int depth) {
   int d1 = depth+1;
   rpt_title("Sleep adjustments on current thread: ", depth);
   Dynamic_Sleep_Adjustment_Stats * stats = dsa_get_stats_t();
   rpt_vstring(d1, "Total successful reads:       %5d", stats->total_ok);
   rpt_vstring(d1, "Total reads with DDC error:   %5d", stats->total_error);
   rpt_vstring(d1, "Total ignored status codes:   %5d", stats->other_status_ct);
   rpt_vstring(d1, "Number of adjustments:        %5d", stats->adjustment_ct);
   rpt_vstring(d1, "Number of excess adjustments: %5d", stats->max_adjustment_ct);
   rpt_vstring(d1, "Final sleep adjustment:       %5.2f", stats->current_sleep_adjustment_factor);
}

#endif
