/** @file display_sleep_data.c
 *
 *  Struct Thread_Sleep_Data maintains all per-display sleep data.
 *
 *  This file contains the usual access and report functions, along with
 *  small functions for managing various fields.
 */

// Copyright (C) 2020-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later
 
#include <assert.h>
#include <sys/types.h>

// for syscall
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#include "util/debug_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/parms.h"
#include "base/core.h"
#include "base/displays.h"
#include "base/rtti.h"
#include "base/sleep.h"
#include "base/per_display_data.h"
#include "base/per_thread_data.h"

#include "ddc/ddc_displays.h"    // !!!

#include "base/display_sleep_data.h"

static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_BASE;

//
// Sleep time adjustment
//

/* Two multipliers are applied to the sleep time determined from the
 * IO mode and event type.
 *
 * sleep_multiplier_factor: set globally, e.g. from arg passed on
 * command line.  Consider making display specific.
 *
 * sleep_multiplier_ct: Per display adjustment,initiated by IO retries.
 */

// Defaults for new displays.  Default sleep multiplier factor can be adjusted,
// Default sleep multiplier count cannot.
static       double default_sleep_multiplier_factor = 1.0; // may be changed by --sleep-multiplier option
static const int    default_sleep_multiplier_count  = 1;
static       bool   default_dynamic_sleep_enabled   = false;


//
// Reporting
//

/** Output a report of the sleep data in a #Per_Display_Data struct in a form
 * intended to be incorporated in program output.
 *
 *  @param  data   pointer to #Per_Display_Data struct
 *  @param  depth  logical indentation level
 */
void report_display_sleep_data(Per_Display_Data * data, int depth) {
   pdd_cross_display_operation_block();
   int d1 = depth+1;
   int d2 = depth+2;
//   rpt_vstring(depth, "Thread %d sleep data:", data->display_id);
   rpt_vstring(depth, "Sleep data for display %s", dpath_repr_t(&data->dpath));
   rpt_label(  d1,   "General:");
   // if (data->dref)
   //    rpt_vstring(d2,    "Display:                           %s", dref_repr_t(data->dref) );
   rpt_vstring(d2,    "Total sleep time (millis):         %d",   data->total_sleep_time_millis);
   rpt_vstring(d2,    "Current sleep-multiplier factor:  %5.2f", data->sleep_multiplier_factor);
   rpt_vstring(d2,    "Dynamic sleep enabled:             %s",   sbool(data->dynamic_sleep_enabled));

   rpt_label(  d1,    "Sleep multiplier adjustment:");
   rpt_vstring(d2,    "Current adjustment:                %d",   data->sleep_multiplier_ct);
   rpt_vstring(d2,    "Highest adjustment:                %d",   data->highest_sleep_multiplier_ct);
   rpt_label(  d2,    "Number of function calls");
   rpt_vstring(d2,    "   that performed adjustment:      %d",   data->sleep_multipler_changer_ct);

   if ( data->dynamic_sleep_enabled ) {
      rpt_label(  d1, "Dynamic Sleep Adjustment:  ");
      rpt_vstring(d2, "Total successful reads:           %5d",   data->total_ok_status_count);
      rpt_vstring(d2, "Total reads with DDC error:       %5d",   data->total_error_status_count);
      rpt_vstring(d2, "Total ignored status codes:       %5d",   data->total_other_status_ct);
      rpt_vstring(d2, "Current sleep adjustment factor:  %5.2f", data->cur_sleep_adjustment_factor);
//    rpt_vstring(d2, "Thread adjustment increment:      %5.2f", data->display_adjustment_increment);
      rpt_vstring(d2, "Adjustment check interval         %5d",   data->adjustment_check_interval);

      rpt_vstring(d2, "Calls since last check:           %5d",   data->calls_since_last_check);
      rpt_vstring(d2, "Total adjustment checks:          %5d",   data->total_adjustment_checks);
      rpt_vstring(d2, "Number of adjustments:            %5d",   data->total_adjustment_ct);
//    rpt_vstring(d2, "Number of excess adjustments:     %5d",   data->total_max_adjustment_ct);
      rpt_vstring(d2, "Final sleep adjustment:           %5.2f", data->cur_sleep_adjustment_factor);
   }
}


/** Wraps #report_display_sleep_data() in a form usable by #pdd_apply_all_sorted()
 *
 *  @param  data pointer to #Per_Display_Data instance
 *  @param  arg  an integer holding the depth argument
 */
void wrap_report_display_sleep_data(Per_Display_Data * data, void * arg) {
   int depth = GPOINTER_TO_INT(arg);
   // rpt_vstring(depth, "Per_Display_Data:");  // needed?
   report_display_sleep_data(data, depth);
}


/** Report all #Per_Display_Data structs.  Note that this report includes
 *  structs for displays that have been closed.
 *
 *  @param depth  logical indentation depth
 */
void report_all_display_sleep_data(int depth) {
   // int d1 = depth+1;
   bool debug = false;
   DBGMSF(debug, "Starting");
   assert(per_display_data_hash);
   rpt_label(depth, "Per display sleep data");
   // For debugging per-display locks
   // rpt_vstring(d1, "ptd lock count:                  %d", pdd_lock_count);
   // rpt_vstring(d1, "pdd_unlock_count:                %d", pdd_unlock_count);
   // rpt_vstring(d1, "cross display operations blocked: %d", cross_display_operation_blocked_count);

   pdd_apply_all_sorted(&wrap_report_display_sleep_data, GINT_TO_POINTER(depth+1) );
   DBGMSF(debug, "Done");
   rpt_nl();
}


//
// Obtain, initialize, and reset sleep data for current display
//


Per_Display_Data * dsd_get_display_sleep_data() {
#ifdef OLD
   Per_Display_Data * ptd = pdd_get_per_display_data();
   // n. init_display_sleep_data() is called from per-display data initializer
#endif
   Per_Thread_Data * ptd = ptd_get_per_thread_data();
   Per_Display_Data * pdd = NULL;
   if (ptd->cur_dh) {
      pdd = ptd->cur_dh->dref->pdd;
      assert(pdd->display_sleep_data_defined);
   }
   return pdd;
}



// initialize a single instance, called from init_per_display_data()
void dsd_init_display_sleep_data(Per_Display_Data * data) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "data=%p", (void*)data);

   data->initialized = true;
   data->dynamic_sleep_enabled       = default_dynamic_sleep_enabled;
   data->sleep_multiplier_ct         = default_sleep_multiplier_count;
   data->highest_sleep_multiplier_ct = 1;



   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
         "Setting data->sleep_multiplier_factor = default_sleep_multiplier_factor = %6.3f",
         default_sleep_multiplier_factor);
   data->sleep_multiplier_factor      = default_sleep_multiplier_factor;
   // data->display_adjustment_increment = default_sleep_multiplier_factor;
   data->cur_sleep_adjustment_factor  = 1.0;
   data->adjustment_check_interval    = 2;
   data->total_sleep_time_millis      = 0;
   data->display_sleep_data_defined   = true;   // vs data->initialized

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "sleep_multiplier_factor = %5.2f", data->sleep_multiplier_factor);
}


#ifdef UNUSED
void reset_display_sleep_data(Per_Display_Data * data) {
   pdd_cross_display_operation_block();
   data->highest_sleep_multiplier_ct = data->sleep_multiplier_ct;
   data->sleep_multipler_changer_ct = 0;
   data->total_ok_status_count = 0;
   data->total_error_status_count = 0;
   data->total_other_status_ct = 0;
   data->total_adjustment_checks = 0;
   data->total_adjustment_ct = 0;
   data->total_max_adjustment_ct = 0;
   data->total_sleep_time_millis = 0;
}


void wrap_reset_display_sleep_data(Per_Display_Data * data, void * arg) {
   reset_display_sleep_data(data);
}
#endif


//
// Operations on all instances
//

#ifdef UNUSED
void reset_all_display_sleep_data() {
   if (per_display_data_hash) {
      pdd_apply_all_sorted(&wrap_reset_display_sleep_data, NULL );
   }
}
#endif


//
// Sleep time adjustment
//

/* Two multipliers are applied to the sleep time determined from the
 * io mode and event type.
 *
 * A default sleep_multiplier_factor: is set globally,
 * e.g. from the --sleep-multiplier option passed on command line.
 * It can be adjusted on a per display basis..
 *
 * The sleep multiplier count is intended for short-term dynamic
 * adjustment, typically by retry mechanisms within a single operation.
 * It is normally 1.
 */

//
// Sleep Multiplier Factor
//

/** Sets the default sleep multiplier factor, used for the creation of any new displays.
 * This is a global value and is a floating point number.
 *
 *  @param multiplier
 *
 *  @remark Intended for use only during program initialization.  If used
 *          more generally, get and set of default sleep multiplier needs to
 *          be protected by a lock.
 *  @todo
 *  Add Sleep_Event_Type bitfield to make sleep factor dependent on event type?
 */
void dsd_set_default_sleep_multiplier_factor(double multiplier) {
   bool debug = false;
   DBGTRC(debug, DDCA_TRC_NONE,
                    "Executing. Setting default_sleep_multiplier_factor = %6.3f", multiplier);
   assert(multiplier >= 0);
   default_sleep_multiplier_factor = multiplier;
}


/** Gets the default sleep multiplier factor.
 *
 *  @return sleep multiplier factor
 */
double dsd_get_default_sleep_multiplier_factor() {
   bool debug = false;
   DBGTRC(debug, DDCA_TRC_NONE,
          "Returning default_sleep_multiplier_factor = %6.3f", default_sleep_multiplier_factor);
   return default_sleep_multiplier_factor;
}


/** Gets the sleep multiplier factor for the current display.
 *
 *  @return sleep multiplier factor
 */
double dsd_get_sleep_multiplier_factor(Per_Display_Data * data) {
   bool debug = false;
   // Per_Display_Data * data = dsd_get_display_sleep_data();
   double result = data->sleep_multiplier_factor;
   DBGTRC(debug, TRACE_GROUP, "Returning %6.3f", result );
   return result;
}


/** Sets the sleep multiplier factor for the current display.
 *
 *  @param factor  sleep multiplier factor
 */
void dsd_set_sleep_multiplier_factor(Per_Display_Data * data, double factor) {
   bool debug = false;

   // Need to guard with mutex!

   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "factor = %6.3f", factor);
   assert(factor >= 0);
   // pdd_cross_display_operation_block();
   // Per_Display_Data * data = dsd_get_display_sleep_data();
   data->sleep_multiplier_factor = factor;
   // data->display_adjustment_increment = factor;
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


#ifdef UNUSED
default_dynamic_sleep_enabled

void set_global_sleep_multiplier_factor(double factor) {
   bool debug = false;
   DBGMSF(debug, "factor = %5.2f", factor);
   global_sleep_multiplier_factor = factor;
   // set_sleep_multiplier_factor_all(factor);   // only applies to new displays, do not change existing displays
}


double get_global_sleep_multiplier_factor() {
   return global_sleep_multiplier_factor;
}
#endif


//
// Sleep Multiplier Count
//

/** Gets the multiplier count for the current display.
 *
 *  @return multiplier count
 */
int dsd_get_sleep_multiplier_ct(Per_Display_Data * data) {
   // Per_Display_Data * data = dsd_get_display_sleep_data();
   return data->sleep_multiplier_ct;
}


/** Sets the multiplier count for the current display.
 *
 *  @param multipler_ct  value to set
 */
void dsd_set_sleep_multiplier_ct(Per_Display_Data * data , int multiplier_ct) {
   bool debug = false;
   DBGMSF(debug, "Setting sleep_multiplier_ct = %d for current display", multiplier_ct);
   assert(multiplier_ct > 0 && multiplier_ct < 100);
   // pdd_cross_display_operation_start();
   // Per_Display_Data * data = dsd_get_display_sleep_data();
   data->sleep_multiplier_ct = multiplier_ct;
   if (multiplier_ct > data->highest_sleep_multiplier_ct)
      data->highest_sleep_multiplier_ct = multiplier_ct;
   pdd_cross_display_operation_end();
}


/** Increment the number of function executions on this display
 *  that changed the sleep multiplier count.
 */
void dsd_bump_sleep_multiplier_changer_ct(Per_Display_Data * data) {
   // pdd_cross_display_operation_block();
   // Per_Display_Data * data = dsd_get_display_sleep_data();
   data->sleep_multipler_changer_ct++;
}


#ifdef UNUSED
// apply the sleep-multiplier to any existing displays
// it will be set for new displays from global_sleep_multiplier_factor
void set_sleep_multiplier_factor_all(double factor) {
   // needs mutex
   bool debug = false;
   DBGMSF(debug, "Starting. factor = %5.2f", factor);
   if (display_sleep_data_hash) {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init (&iter,display_sleep_data_hash);
      while (g_hash_table_iter_next (&iter, &key, &value)) {
         Per_Display_Data * data = value;
         DBGMSF(debug, "Thread id: %d", data->display_id);
         data->sleep_multiplier_factor = factor;
      }
   }
}
#endif


//
// Dynamic Sleep
//

#ifdef UNUSED
/** Enable or disable dynamic sleep adjustment on the current display
 *
 *  @param enabled   true/false i.e. enabled, disabled
 */
void dsd_enable_dynamic_sleep(bool enabled) {
   bool debug = false;
   DBGMSF(debug, "enabled = %s", sbool(enabled));
   pdd_cross_display_operation_start();
   // bool this_function_owns_lock = pdd_lock_if_unlocked();
   Per_Display_Data * data = dsd_get_display_sleep_data();
   data->dynamic_sleep_enabled = enabled;
   pdd_cross_display_operation_end();
   // pdd_unlock_if_needed(this_function_owns_lock);
}
#endif


/** Enable or disable dynamic sleep adjustment on all existing displays
 *
 *  @param enable  true/false
 */
void dsd_enable_dsa_all(bool enable) {
   bool debug = false;
   DBGMSF(debug, "enable=%s", sbool(enable));
   // needs mutex
   pdd_cross_display_operation_start();
   DBGMSF(debug, "Starting. enable = %s", sbool(enable) );
   default_dynamic_sleep_enabled = enable;  // for initializing new displays

   if (per_display_data_hash) {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init (&iter,per_display_data_hash);
      while (g_hash_table_iter_next (&iter, &key, &value)) {
         Per_Display_Data * data = value;
         DBGMSF(debug, "Display: %s", dpath_repr_t(&data->dpath));
         data->dynamic_sleep_enabled = enable;
      }
   }

#ifdef NEW
   GPtrArray * drefs = ddc_get_all_displays();
   for (int ndx = 0; ndx < drefs->len; ndx++) {
      Display_Ref * dref = g_ptr_array_index(drefs, ndx);
      Per_Display_Data * pdd = dref->pdd;
      pdd->dynamic_sleep_enabled = enable;
   }
#endif


   pdd_cross_display_operation_end();
}


/** Enable or disable dynamic sleep adjustment for the current display.
 *
 *  @param true/false
 */
void dsd_dsa_enable(bool enabled) {
#ifdef OUT
   pdd_cross_display_operation_block();
   Per_Display_Data * tsd = dsd_get_display_sleep_data();
   tsd->dynamic_sleep_enabled = enabled;
#endif
}


/** Enable or disable dynamic sleep adjustment for all current displays and new displays
 *
 *  @param enabled  true/false
 */
void dsd_dsa_enable_globally(bool enabled) {
   bool debug = false;
   DBGMSF(debug, "Executing.  enabled = %s", sbool(enabled));
   pdd_cross_display_operation_start();
   default_dynamic_sleep_enabled = enabled;
   dsd_enable_dsa_all(enabled) ;
   pdd_cross_display_operation_end();
}


#ifdef UNUSED
// Is dynamic sleep enabled on the current display?
bool dsd_dsa_is_enabled() {
   pdd_cross_display_operation_start();     // needed
   Per_Display_Data * tsd = dsd_get_display_sleep_data();
   bool result = tsd->dynamic_sleep_enabled;
   pdd_cross_display_operation_end();
   return result;
}
#endif

#ifdef UNUSED
void dsd_set_dsa_enabled_default(bool enabled) {
   default_dynamic_sleep_enabled = enabled;
}
#endif


bool dsd_get_dsa_enabled_default() {
   return default_dynamic_sleep_enabled;
}


void init_display_sleep_data() {
   RTTI_ADD_FUNC(dsd_init_display_sleep_data);
   RTTI_ADD_FUNC(dsd_get_default_sleep_multiplier_factor);
   RTTI_ADD_FUNC(dsd_set_default_sleep_multiplier_factor);
   RTTI_ADD_FUNC(dsd_get_sleep_multiplier_factor);
   RTTI_ADD_FUNC(dsd_set_sleep_multiplier_factor);
}

