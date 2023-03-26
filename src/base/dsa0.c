/** @file dsa0.c
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

#include "base/core.h"
#include "base/displays.h"
#include "base/parms.h"
#include "base/per_display_data.h"
#include "base/rtti.h"
#include "base/sleep.h"

#include "base/dsa0.h"


// static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_BASE;   // unused


bool dsa0_enabled = false;

//
// Sleep time adjustment - old notes
//


void dsa0_enable(bool enabled) {
   dsa0_enabled = enabled;
}

bool dsa0_is_enabledt() {
   return dsa0_enabled;
}



/* Two multipliers are applied to the sleep time determined from the
 * IO mode and event type.
 *
 * sleep_multiplier_factor: set globally, e.g. from arg passed on
 * command line.  Consider making display specific.
 *
 * sleep_multiplier_ct: Per display adjustment,initiated by IO retries.
 */


DSA0_Data ** dsa0_data_recs;


DSA0_Data * new_dsa0_data(int busno) {
   DSA0_Data * result = calloc(1,sizeof(DSA0_Data));
   result->busno = busno;
   result->sleep_multiplier_ct = 1;
   result->highest_sleep_multiplier_ct = 1;
   return result;
}


/** Returns the #Results_Table for an I2C bus number
 *
 *  @param  bus number
 *  @return pointer to #Results_Table (may be newly created)
 */
DSA0_Data * dsa0_get_dsa0_data(int busno) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "busno=%d", busno);
   assert(busno <= I2C_BUS_MAX);
   DSA0_Data * dsa0_data = dsa0_data_recs[busno];
   if (!dsa0_data) {
      dsa0_data = new_dsa0_data(busno);
      dsa0_data_recs[busno] =   dsa0_data;
   }
   DBGTRC_DONE(debug, DDCA_TRC_NONE, "Returning   dsa0_data=%p", dsa0_data);
   return dsa0_data;
}



void dsa0_reset(DSA0_Data * dsa0) {
   dsa0->sleep_multiplier_ct = 1   ;         // can be changed by retry logic - this is a factor
   dsa0->highest_sleep_multiplier_ct = 1;     // high water mark
   dsa0->sleep_multiplier_changer_ct = 0;      // number of function calls that adjusted multiplier ct
   dsa0->adjusted_sleep_multiplier = 1.0;
}


double  dsa0_get_adjusted_sleep_multiplier(DSA0_Data * dsa0) {
   return dsa0->adjusted_sleep_multiplier;
}

void dsa0_note_retryable_failure(DSA0_Data* dsa0, int remaining_tries) {
   dsa0->sleep_multiplier_ct++;
   if (dsa0->sleep_multiplier_ct > dsa0->highest_sleep_multiplier_ct)
      dsa0->highest_sleep_multiplier_ct = dsa0->sleep_multiplier_ct;
   dsa0->sleep_multiplier_changer_ct++;
   dsa0->adjusted_sleep_multiplier = dsa0->sleep_multiplier_ct;
}

void         dsa0_record_final_by_pdd(Per_Display_Data * pdd,  DDCA_Status ddcrc, int retries) {
}







//
// Reporting
//

/** Output a report of the sleep data in a #Per_Display_Data struct in a form
 * intended to be incorporated in program output.
 *
 *  @param  data   pointer to #Per_Display_Data struct
 *  @param  depth  logical indentation level
 */
void report_dsa0_data(DSA0_Data * data, int depth) {
   pdd_cross_display_operation_block(__func__);
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_label(  d1,   "Dynamic sleep adjustment detail (algorithm 0):");
   // if (data->dref)
   //    rpt_vstring(d2,    "Display:                           %s", dref_repr_t(data->dref) );
#ifdef USE_DSA0
   rpt_vstring(d2,    "Total sleep time (millis):         %d",   data->total_sleep_time_millis);   // not defined
#endif
//   rpt_vstring(d2,    "Current sleep-multiplier factor:  %5.2f", data->sleep_multiplier_factor);
   rpt_label(  d1,    "Sleep multiplier adjustment:");
   rpt_vstring(d2,    "Current adjustment:                %d",   data->sleep_multiplier_ct);
   rpt_vstring(d2,    "Highest adjustment:                %d",   data->highest_sleep_multiplier_ct);
   rpt_label(  d2,    "Number of function calls");
   rpt_vstring(d2,    "   that performed adjustment:      %d",   data->sleep_multiplier_changer_ct);
}


/** Wraps #report_display_sleep_data() in a form usable by #pdd_apply_all_sorted()
 *
 *  @param  data pointer to #Per_Display_Data instance
 *  @param  arg  an integer holding the depth argument
 */
void wrap_report_dsa0_data(DSA0_Data * data, void * arg) {
   int depth = GPOINTER_TO_INT(arg);
   // rpt_vstring(depth, "Per_Display_Data:");  // needed?
   report_dsa0_data(data, depth);
}

void dsa0_apply_all(Dsa0_Func func, void * arg) {
   pdd_cross_display_operation_start(__func__);
   // bool debug = false;
   // assert(per_display_data_hash);    // allocated by init_display_data_module()

   for (int ndx = 0; ndx <= I2C_BUS_MAX; ndx++) {
      if (dsa0_data_recs[ndx]) {
        DSA0_Data * data = dsa0_data_recs[ndx];
       //  DBGMSF(debug, "Thread id: %d", data->dpath);
         func(data, arg);
      }
   }
   pdd_cross_display_operation_end(__func__);
}




/** Report all #Per_Display_Data structs.  Note that this report includes
 *  structs for displays that have been closed.
 *
 *  @param depth  logical indentation depth
 */
void report_all_dsa0_data(int depth) {
   // int d1 = depth+1;
   bool debug = false;
   DBGMSF(debug, "Starting");
   assert(per_display_data_hash);
   rpt_label(depth, "(dsa0) Per display sleep data");
   // For debugging per-display locks
   // rpt_vstring(d1, "ptd lock count:                  %d", pdd_lock_count);
   // rpt_vstring(d1, "pdd_unlock_count:                %d", pdd_unlock_count);
   // rpt_vstring(d1, "cross display operations blocked: %d", cross_display_operation_blocked_count);

   dsa0_apply_all(&wrap_report_dsa0_data, GINT_TO_POINTER(depth+1));   // TEMP NOT SORTED
   // pdd_apply_all_sorted(&wrap_report_dsa0_data, GINT_TO_POINTER(depth+1) );
   DBGMSF(debug, "Done");
   rpt_nl();
}


//
// Obtain, initialize, and reset sleep data for a display
//

// initialize a single instance, called from init_per_display_data()
void dsa0_init_dsa0_data(DSA0_Data * data) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "data=%p, busno=%d", (void*)data, data->busno);

   data->sleep_multiplier_ct          = 1;
   data->highest_sleep_multiplier_ct  = 1;
   data->sleep_multiplier_changer_ct  = 0;
   data->adjusted_sleep_multiplier    = 1.0;

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


//
// Operations on all instances
//




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
// Sleep Multiplier Count
//

/** Sets the multiplier count for the specified display.
 *
 *  @param multipler_ct  value to set
 */
void dsa0_set_sleep_multiplier_ct(DSA0_Data * data , int multiplier_ct) {
   bool debug = true;
   DBGMSF(debug, "Setting sleep_multiplier_ct = %d for current display on bus %d", multiplier_ct, data->busno);
   assert(multiplier_ct > 0 && multiplier_ct < 100);
   pdd_cross_display_operation_start(__func__);
   data->sleep_multiplier_ct = multiplier_ct;
   if (multiplier_ct > data->highest_sleep_multiplier_ct)
      data->highest_sleep_multiplier_ct = multiplier_ct;
   pdd_cross_display_operation_end(__func__);
}


/** Increment the number of function executions on a specified display
 *  that changed the sleep multiplier count.
 */
void dsa0_bump_sleep_multiplier_changer_ct(DSA0_Data * data) {
   // pdd_cross_display_operation_block();
   bool debug = true;
   data->sleep_multiplier_changer_ct++;
   DBGMSF(debug, "Executing.  New changer ct = %d", data->sleep_multiplier_changer_ct);
}


//
// Dynamic Sleep
//






void init_dsa0() {
   RTTI_ADD_FUNC(dsa0_init_dsa0_data);
//    RTTI_ADD_FUNC(dsd_get_default_sleep_multiplier_factor);
//    RTTI_ADD_FUNC(dsd_set_default_sleep_multiplier_factor);
//   RTTI_ADD_FUNC(dsd_get_sleep_multiplier_factor);
   // RTTI_ADD_FUNC(_set_sleep_multiplier_factor);

   dsa0_data_recs = calloc(I2C_BUS_MAX+1, sizeof(DSA0_Data*));
}

