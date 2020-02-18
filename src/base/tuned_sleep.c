/** @file tuned_sleep.c
 *
 *  Perform sleep. The sleep time is determined by io mode, sleep event time,
 *  and applicable multipliers.
 */

// Copyright (C) 2019-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <sys/types.h>

// for syscall
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#include "util/debug_util.h"
#include "util/report_util.h"

#include "base/parms.h"
#include "base/dynamic_sleep.h"
#include "base/execution_stats.h"
#include "base/sleep.h"


//
// Sleep time adjustment
//

/* Two multipliers are applied to the sleep time determined from the
 * io mode and event type.
 *
 * sleep_multiplier_factor: set globally, e.g. from arg passed on
 * command line.  Consider making thread specific.
 *
 * sleep_multiplier_ct: Per thread adjustment,initiated by io retries.
 */

// sleep-multiplier value passed on command line
static double sleep_multiplier_factor = 1.0;

/** Sets the sleep multiplier factor.  This is a global value and is a floating
 *  point number.
 *
 *  \param multiplier
 *
 *  \todo
 *  Add Sleep_Event_Type bitfield to make sleep factor dependent on event type?
 *  Make thread specific?
 */
void   set_sleep_multiplier_factor(double multiplier) {
   bool debug = false;
   DBGMSF(debug, "Setting sleep_multiplier_factor = %6.2f", multiplier);
   assert(multiplier > 0 && multiplier < 100);
   sleep_multiplier_factor = multiplier;
   dsa_set_sleep_multiplier_factor(multiplier);
}

/** Gets the current sleep multiplier factor.
 *
 *  \return sleep multiplier factor
 */
double get_sleep_multiplier_factor() {
   return sleep_multiplier_factor;
}


typedef struct {
   pid_t  thread_id;
   int    sleep_multiplier_ct;   // thread specific since can be changed dynamically
   int    max_sleep_multiplier_ct;
   int    sleep_multiplier_changed_ct;
} Thread_Sleep_Settings;


static GHashTable * thread_sleep_settings_hash = NULL;

// static
void register_thread_sleep_settings(Thread_Sleep_Settings * per_thread_settings) {
   // NEED MUTEX
   if (!thread_sleep_settings_hash) {
      thread_sleep_settings_hash = g_hash_table_new(g_direct_hash, NULL);
   }
   assert(!g_hash_table_contains(thread_sleep_settings_hash,
                                 GINT_TO_POINTER(per_thread_settings->thread_id)));
   g_hash_table_insert(thread_sleep_settings_hash,
                       GINT_TO_POINTER(per_thread_settings->thread_id),
                       per_thread_settings);

}


static Thread_Sleep_Settings *  get_thread_sleep_settings() {
   static GPrivate per_thread_key = G_PRIVATE_INIT(g_free);

   Thread_Sleep_Settings *settings = g_private_get(&per_thread_key);

   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, settings=%p\n", __func__, this_thread, settings);

   if (!settings) {
      settings = g_new0(Thread_Sleep_Settings, 1);
      settings->thread_id = syscall(SYS_gettid);
      settings->sleep_multiplier_ct = 1;
      settings->max_sleep_multiplier_ct = 1;
      g_private_set(&per_thread_key, settings);
   }

   // printf("(%s) Returning: %p\n", __func__, settings);
   return settings;
}


/** Gets the multiplier count for the current thread.
 *
 *  \return multiplier count
 */
int get_sleep_multiplier_ct() {
   Thread_Sleep_Settings * settings = get_thread_sleep_settings();
   return settings->sleep_multiplier_ct;
}


/** Sets the multiplier count for the current thread.
 *
 *  \parsm multipler_ct  value to set
 */
void   set_sleep_multiplier_ct(/* Sleep_Event_Type event_types,*/ int multiplier_ct) {
   assert(multiplier_ct > 0 && multiplier_ct < 100);
   Thread_Sleep_Settings * settings = get_thread_sleep_settings();
   settings->sleep_multiplier_ct = multiplier_ct;
   if (multiplier_ct > settings->max_sleep_multiplier_ct)
      settings->max_sleep_multiplier_ct = multiplier_ct;
   // DBGMSG("Setting sleep_multiplier_ct = %d", settings->sleep_multiplier_ct);
}

void bump_sleep_multiplier_changed_ct() {
   Thread_Sleep_Settings * settings = get_thread_sleep_settings();
   settings->sleep_multiplier_changed_ct++;
}

void report_thread_sleep_settings(Thread_Sleep_Settings * settings, int depth) {
   int d1 = depth+1;
   rpt_vstring(depth, "Per thread sleep stats for thread %d", settings->thread_id);
   rpt_vstring(d1   , "Max sleep multiplier count:     %d", settings->max_sleep_multiplier_ct);
   rpt_vstring(d1   , "Number of retry function calls that increased multiplier_count: %d",
                         settings->sleep_multiplier_changed_ct);
}


// typedef GFunc
void report_one_hash_settings(
      gpointer key,
      gpointer value,
      gpointer user_data)
{
   // int thread_id = (int) key;
   Thread_Sleep_Settings * settings = value;
   int depth = GPOINTER_TO_INT(user_data);
   report_thread_sleep_settings(settings, depth);
   rpt_nl();
}



void report_all_thread_sleep_settings(int depth) {
   if (!thread_sleep_settings_hash) {
      rpt_vstring(depth, "No per thread DDC_NULL_MESSAGE sleep statistics found");
   }
   else {
      rpt_vstring(depth, "Per thread DDC_NULL_MESSAGE sleep statistics:");
      g_hash_table_foreach(
            thread_sleep_settings_hash, report_one_hash_settings, GINT_TO_POINTER(depth+1));
   }
}




static bool sleep_suppression_enabled = false;

void enable_sleep_suppression(bool enable) {
   sleep_suppression_enabled = enable;
}


//
// Perform sleep
//

/** Sleep for the period of time required by the DDC protocol, as indicated
 *  by the io mode and sleep event type.
 *
 *  The time is further adjusted by the sleep factor and sleep multiplier
 *  currently in effect.
 *
 *  \todo
 *  Take into account the time since the last monitor return in the
 *  current thread.
 *  \todp
 *  Take into account per-display error statistics.  Would require
 *  error statistics be maintained on a per-display basis, either
 *  in the display reference or display handle.
 *
 * \param io_mode     communication mechanism
 * \param event_type  reason for sleep
 * \param func        name of function that invoked sleep
 * \param lineno      line number in file where sleep was invoked
 * \param filename    name of file from which sleep was invoked
 * \param msg         text to append to trace message
 */
void tuned_sleep_with_tracex(
      DDCA_IO_Mode     io_mode,
      Sleep_Event_Type event_type,
      int              special_sleep_time_millis,
      const char *     func,
      int              lineno,
      const char *     filename,
      const char *     msg)
{
   bool debug = false;
   // DBGMSF(debug, "Starting. Sleep event type = %s", sleep_event_name(event_type));
   assert( (event_type != SE_SPECIAL && special_sleep_time_millis == 0) ||
           (event_type == SE_SPECIAL && special_sleep_time_millis >  0) );

   int sleep_time_millis = 0;    // should be a default

   if (event_type == SE_SPECIAL)
      sleep_time_millis = special_sleep_time_millis;
   else {

      switch(io_mode) {

      case DDCA_IO_I2C:
         switch(event_type) {
         case (SE_WRITE_TO_READ):
               sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
               break;
         case (SE_POST_WRITE):
               sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
               break;
         case (SE_POST_OPEN):
               sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
               break;
         case (SE_POST_READ):
               sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
               if (sleep_suppression_enabled) {
                  DBGMSF(debug, "Suppressing sleep, sleep event type = %s", sleep_event_name(event_type));
                  return;  // TEMP
               }
               break;
         case (SE_POST_SAVE_SETTINGS):
               sleep_time_millis = DDC_TIMEOUT_POST_SAVE_SETTINGS;   // per DDC spec
               break;
         case SE_DDC_NULL:
              sleep_time_millis = DDC_TIMEOUT_MILLIS_NULL_RESPONSE_INCREMENT;
              break;
         case SE_PRE_EDID:
              sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
              if (sleep_suppression_enabled) {
              DBGMSF(debug, "Suppressing sleep, sleep event type = %s",
                            sleep_event_name(event_type));
              return;   // TEMP
              }
              break;
         case SE_OTHER:
              sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
              // if (sleep_suppression_enabled) {
              // DBGMSF(debug, "Suppressing sleep, sleep event type = %s",
              //               sleep_event_name(event_type));
              // return;
              // }
              break;
         case SE_PRE_MULTI_PART_READ:        // before reading capabilitis
            sleep_time_millis = 200;
            break;
         case SE_MULTI_PART_READ_TO_WRITE:
            sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
            break;
         default:
              sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
         }  // switch within DDC_IO_DEVI2C
         break;

      case DDCA_IO_ADL:
         switch(event_type) {
         case (SE_WRITE_TO_READ):
               sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
               break;
         case (SE_POST_WRITE):
               sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
               break;
         case (SE_POST_OPEN):
               sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
               break;
         case (SE_POST_SAVE_SETTINGS):
               sleep_time_millis = 200;   // per DDC spec
               break;
         default:
            sleep_time_millis = DDC_TIMEOUT_MILLIS_DEFAULT;
         }
         break;

      case DDCA_IO_USB:
         PROGRAM_LOGIC_ERROR("call_tuned_sleep() called for USB_IO\n");
         break;
      }
   }

   // TODO:
   //   get error rate (total calls, total errors), current adjustment value
   //   adjust by time since last i2c event

#ifdef NO
   static float sleep_adjustment_factor = 1.0;
   float next_sleep_adjustment_factor = dsa_get_sleep_adjustment();
   if (next_sleep_adjustment_factor > sleep_adjustment_factor) {
      sleep_adjustment_factor = next_sleep_adjustment_factor;
      reset_ddcrw_status_code_counts();
   }
#endif

   double sleep_adjustment_factor = dsa_get_sleep_adjustment();

   // crude, should be sensitive to event type?
   int sleep_multiplier_ct = get_sleep_multiplier_ct();  // per thread
   sleep_time_millis = sleep_multiplier_ct * sleep_multiplier_factor * sleep_time_millis * sleep_adjustment_factor;
   if (debug) {
   // if (sleep_multiplier_factor != 1.0 || sleep_multiplier_ct != 1 || sleep_adjustment_factor != 1.0 ||debug) {
      DBGMSG("Before sleep. event type: %s, sleep_multiplier_ct = %d, sleep_multiplier_factor = %9.1f, sleep_adjustment_factor = %9.1f, sleep_time_millis = %d",
             sleep_event_name(event_type), sleep_multiplier_ct, sleep_multiplier_factor, sleep_adjustment_factor, sleep_time_millis);
      // show_backtrace(2);
   }

   record_sleep_event(event_type);

   char msg_buf[100];
   const char * evname = sleep_event_name(event_type);
   if (msg)
      g_snprintf(msg_buf, 100, "Event type: %s, %s", evname, msg);
   else
      g_snprintf(msg_buf, 100, "Event_type: %s", evname);

   sleep_millis_with_tracex(sleep_time_millis, func, lineno, filename, msg_buf);

   DBGMSF(debug, "Done");
}


//
// Convenience functions
//

#ifdef UNUSED
/** Convenience function that determines the device type from the
 *  #Display_Handle before invoking all_tuned_sleep().
 *  @param dh         display handle of open device
 *  @param event_type sleep event type
 *
 *  \todp
 *  Extend with location parmss
 */
void tuned_sleep_dh(Display_Handle* dh, Sleep_Event_Type event_type) {
   tuned_sleep_with_tracex(dh->dref->io_path.io_mode, event_type, NULL, 0, NULL, NULL);
}
#endif
