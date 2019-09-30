/** @file tuned_sleep.c
 *
 */

// Copyright (C) 2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>

#include "base/parms.h"
#include "base/execution_stats.h"
#include "base/sleep.h"


static bool                 debug_sleep_stats_mutex = false;

typedef struct {
   int    sleep_multiplier_ct;   // thread specific since can be changed dynamically
} Thread_Sleep_Settings;

static Thread_Sleep_Settings *  get_thread_sleep_settings() {
   static GPrivate per_thread_key = G_PRIVATE_INIT(g_free);

   Thread_Sleep_Settings *settings = g_private_get(&per_thread_key);

   // GThread * this_thread = g_thread_self();
   // printf("(%s) this_thread=%p, settings=%p\n", __func__, this_thread, settings);

   if (!settings) {
      settings = g_new0(Thread_Sleep_Settings, 1);
      settings->sleep_multiplier_ct = 1;
      g_private_set(&per_thread_key, settings);
   }

   // printf("(%s) Returning: %p\n", __func__, settings);
   return settings;
}



static double sleep_multiplier_factor = 1.0;
// static int    sleep_multiplier_ct = 1;

int get_sleep_multiplier_ct() {
   Thread_Sleep_Settings * settings = get_thread_sleep_settings();
   return settings->sleep_multiplier_ct;
}

double get_sleep_multiplier_factor() {
   return sleep_multiplier_factor;
}


void   set_sleep_multiplier_ct(/* Sleep_Event_Type event_types,*/ int multiplier_ct) {
   assert(multiplier_ct > 0 && multiplier_ct < 100);
   Thread_Sleep_Settings * settings = get_thread_sleep_settings();
   settings->sleep_multiplier_ct = multiplier_ct;
   // DBGMSG("Setting sleep_multiplier_ct = %d", settings->sleep_multiplier_ct);
}

void   set_sleep_multiplier_factor(/* Sleep_Event_Type event_types,*/ double multiplier) {
   assert(multiplier > 0 && multiplier < 100);
   sleep_multiplier_factor = multiplier;
   // DBGMSG("Setting sleep_multiplier_factor = %6.1f", sleep_multiplier_factor);
}


/** Sleep for a period required by the DDC protocol.
 *
 *  This function allows for tuning the actual sleep time.
 *
 *  This function does 3 things:
 *  1.  Determine the sleep period based on the communication
 *      mechanism, call type, sleep strategy in effect,
 *      and potentially other information.
 *  2. Record the sleep event.
 *  3. Sleep for period determined.
 *
 * @param io_mode     communication mechanism
 * @param event_type  reason for sleep
 *
 * @todo
 * Extend to take account of actual time since return from
 * last system call, previous error rate, etc.
 */
void tuned_sleep_with_trace(DDCA_IO_Mode io_mode, Sleep_Event_Type event_type, char * loc, char * msg) {
   bool debug = false || debug_sleep_stats_mutex;
   DBGMSF(debug, "Starting");

   int sleep_time_millis = 0;    // should be a default
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
            break;
      case (SE_POST_SAVE_SETTINGS):
            sleep_time_millis = DDC_TIMEOUT_POST_SAVE_SETTINGS;   // per DDC spec
            break;
      case SE_DDC_NULL:
          sleep_time_millis = DDC_TIMEOUT_MILLIS_NULL_RESPONSE_INCREMENT;
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

   // TODO:
   //   get error rate (total calls, total errors), current adjustment value
   //   adjust by time since last i2c event
   // Is tracing useful, given that we know the event type?
   // void sleep_millis_with_trace(int milliseconds, const char * caller_location, const char * message);

   // crude, should be sensitive to event type
   int sleep_multiplier_ct = get_sleep_multiplier_ct();  // per thread
   sleep_time_millis = sleep_multiplier_ct * sleep_multiplier_factor * sleep_time_millis;
   if (debug) {
   // if (sleep_multiplier_factor != 1.0 || sleep_multiplier_ct != 1 || debug) {
      DBGMSG("Sleep event type: %s, sleep_multiplier_ct = %d, sleep_multiplier_factor = %9.1f, sleep_time_millis = %d",
             sleep_event_name(event_type), sleep_multiplier_ct, sleep_multiplier_factor, sleep_time_millis);
   }

   record_sleep_event(event_type);

   sleep_millis_with_trace(sleep_time_millis, loc, msg);

   DBGMSF(debug, "Done");
}


void tuned_sleep(DDCA_IO_Mode io_mode, Sleep_Event_Type event_type)
{
   tuned_sleep_with_trace(io_mode, event_type, NULL, NULL);
}


// Convenience functions

/** Convenience function that invokes call_tuned_sleep() for
 *  /dev/i2c devices.
 *
 *  @param event_type sleep event type
 */
void tuned_sleep_i2c(Sleep_Event_Type event_type) {
   tuned_sleep(DDCA_IO_I2C, event_type);
}

#ifdef DEPRECATED
/** Convenience function that invokes call_tuned_sleep() for
 *  ADL devices.
 *
 *  @param event_type sleep event type
 */
void call_tuned_sleep_adl(Sleep_Event_Type event_type) {
   tuned_sleep(DDCA_IO_ADL, event_type);
}
#endif


/** Convenience function that determines the device type from the
 *  #Display_Handle before invoking all_tuned_sleep().
 *  @param dh         display handle of open device
 *  @param event_type sleep event type
 */
void tuned_sleep_dh(Display_Handle* dh, Sleep_Event_Type event_type) {
   tuned_sleep(dh->dref->io_path.io_mode, event_type);
}
