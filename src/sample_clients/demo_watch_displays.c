/** \file demo_watch_displays.c
 *
 * Sample program illustrating libddcutil's display watch functions.
 *
 * Registers a display status callback, starts watching for display changes,
 * and reports each event as it occurs: when it happened, its type, the I2C
 * bus and DRM connector it applies to, the display reference, and whether
 * DDC communication works on a newly connected display.
 *
 * Unlike stress_watch, which counts events while hammering the API from many
 * threads, this program simply shows what each event contains.  It is useful
 * for confirming that hot plug detection works on a particular system, and
 * its output is worth attaching to a problem report.
 *
 * Usage: demo_watch_displays [seconds]     (default 120)
 *
 * Unplug and replug a monitor while it runs.  Terminate early with ctrl-c.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "public/ddcutil_c_api.h"
#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_types.h"

static volatile sig_atomic_t stop_requested = false;
static int      event_ct           = 0;
static uint64_t first_event_nanos  = 0;


static void sigint_handler(int signum) {
   stop_requested = true;
}


/** Formats the io path of a display in a form suitable for display
 *
 *  @param  io_path   display access path
 *  @param  buf       where to return the formatted value
 *  @param  bufsz     buffer size
 *  @return buf
 */
static char * io_path_description(DDCA_IO_Path io_path, char * buf, int bufsz) {
   if (io_path.io_mode == DDCA_IO_I2C)
      snprintf(buf, bufsz, "/dev/i2c-%d", io_path.path.i2c_busno);
   else
      snprintf(buf, bufsz, "/dev/usb/hiddev%d", io_path.path.hiddev_devno);
   return buf;
}


/** Interprets how the DRM connector for a display was determined.
 *
 *  @param  found_by  value to interpret
 *  @return descriptive string, do not free
 */
static const char * connector_found_by_description(DDCA_Drm_Connector_Found_By found_by) {
   switch(found_by) {
   // n.b. DDCA_DRM_CONNECTOR_NOT_FOUND is also reported when the association
   // was specified by the user, i.e. option --bus-drm-connector
   case DDCA_DRM_CONNECTOR_NOT_FOUND:       return "not found or user specified";
   case DDCA_DRM_CONNECTOR_FOUND_BY_BUSNO:  return "I2C bus number";
   case DDCA_DRM_CONNECTOR_FOUND_BY_EDID:   return "EDID";
   }
   return "unrecognized";
}


/** Interprets a #DDCA_Display_Event_Class bit mask
 *
 *  @param  classes   value to interpret
 *  @param  buf       where to return the formatted value
 *  @param  bufsz     buffer size
 *  @return buf
 */
static char * event_classes_description(
      DDCA_Display_Event_Class classes, char * buf, int bufsz)
{
   snprintf(buf, bufsz, "%s%s%s",
            (classes & DDCA_EVENT_CLASS_DPMS)               ? "DPMS " : "",
            (classes & DDCA_EVENT_CLASS_DISPLAY_CONNECTION) ? "DISPLAY_CONNECTION " : "",
            (classes) ? "" : "none");
   return buf;
}


/** Interprets the flags field of a #DDCA_Display_Status_Event
 *
 *  @param  flags     value to interpret
 *  @param  buf       where to return the formatted value
 *  @param  bufsz     buffer size
 *  @return buf
 */
static char * event_flags_description(uint8_t flags, char * buf, int bufsz) {
   uint8_t unrecognized = flags & ~DDCA_DISPLAY_EVENT_DDC_WORKING;
   const char * ddc_working = (flags & DDCA_DISPLAY_EVENT_DDC_WORKING) ? "DDC working" : "";
   if (unrecognized)
      snprintf(buf, bufsz, "%s%s0x%02x",
               ddc_working, (*ddc_working) ? ", " : "", unrecognized);
   else
      snprintf(buf, bufsz, "%s", (*ddc_working) ? ddc_working : "none");
   return buf;
}


/** Display status callback function.
 *
 *  Invoked by libddcutil on one of its own threads, not on the thread that
 *  registered the function.  Note that the event record is passed on the
 *  stack.  There is nothing for the client to free.
 *
 *  @param event  event record
 */
static void display_status_callback(DDCA_Display_Status_Event event) {
   char path_buf[40];
   char flag_buf[40];

   if (first_event_nanos == 0)
      first_event_nanos = event.timestamp_nanos;
   double elapsed_sec = (event.timestamp_nanos - first_event_nanos) / (1000.0 * 1000.0 * 1000.0);

   event_ct++;
   printf("[%8.3f] event %d: %-32s %-16s connector: %-16s dref: %p  flags: %s\n",
          elapsed_sec,
          event_ct,
          ddca_display_event_type_name(event.event_type),
          io_path_description(event.io_path, path_buf, sizeof(path_buf)),
          (event.connector_name[0]) ? event.connector_name : "(none)",
          event.dref,
          event_flags_description(event.flags, flag_buf, sizeof(flag_buf)));
   fflush(stdout);   // output is typically watched as it appears
}


/** Reports the displays that exist when watching begins, for comparison
 *  with the events subsequently reported.
 */
static void report_initial_displays() {
   DDCA_Display_Ref * drefs = NULL;
   DDCA_Status ddcrc = ddca_get_display_refs(false, &drefs);
   if (ddcrc != 0) {
      fprintf(stderr, "ddca_get_display_refs() returned %d (%s)\n", ddcrc, ddca_rc_name(ddcrc));
      return;
   }

   printf("Displays detected:\n");
   for (int ndx = 0; drefs[ndx]; ndx++) {
      DDCA_Display_Info2 * info = NULL;
      ddcrc = ddca_get_display_info2(drefs[ndx], &info);
      if (ddcrc == 0) {
         char path_buf[40];
         printf("   %-16s connector: %-16s id: %-5d %s %s  (connector found by: %s)\n",
                io_path_description(info->path, path_buf, sizeof(path_buf)),
                (info->drm_card_connector[0]) ? info->drm_card_connector : "(none)",
                info->drm_connector_id,
                info->mfg_id,
                info->model_name,
                connector_found_by_description(info->drm_card_connector_found_by));
         ddca_free_display_info2(info);
      }
   }
   free(drefs);
}


int main(int argc, char** argv) {
   int watch_seconds = (argc > 1) ? atoi(argv[1]) : 120;
   if (watch_seconds <= 0) {
      fprintf(stderr, "Usage: %s [seconds]\n", argv[0]);
      return 1;
   }
   signal(SIGINT, sigint_handler);

   DDCA_Status ddcrc = ddca_init2(NULL, DDCA_SYSLOG_NOTICE, DDCA_INIT_OPTIONS_NONE, NULL);
   if (ddcrc != 0) {
      fprintf(stderr, "ddca_init2() failed: %d (%s)\n", ddcrc, ddca_rc_name(ddcrc));
      return 1;
   }

   report_initial_displays();

   ddcrc = ddca_register_display_status_callback(display_status_callback);
   if (ddcrc != 0) {
      fprintf(stderr, "ddca_register_display_status_callback() failed: %d (%s)\n",
                      ddcrc, ddca_rc_name(ddcrc));
      return 1;
   }

   ddcrc = ddca_start_watch_displays(DDCA_EVENT_CLASS_ALL);
   if (ddcrc != 0) {
      fprintf(stderr, "ddca_start_watch_displays() failed: %d (%s)\n",
                      ddcrc, ddca_rc_name(ddcrc));
      return 1;
   }

   // The classes actually being watched can be fewer than those requested.
   DDCA_Display_Event_Class active_classes = 0;
   ddcrc = ddca_get_active_watch_classes(&active_classes);
   if (ddcrc == 0) {
      char class_buf[60];
      printf("Watching event classes: %s\n",
             event_classes_description(active_classes, class_buf, sizeof(class_buf)));
   }

   printf("Watching for %d seconds.  Unplug and replug a monitor.  ctrl-c to stop.\n",
          watch_seconds);
   fflush(stdout);

   for (int ndx = 0; ndx < watch_seconds && !stop_requested; ndx++)
      sleep(1);

   printf("\n%d event(s) reported\n", event_ct);

   ddca_stop_watch_displays(true);
   ddca_unregister_display_status_callback(display_status_callback);
   return 0;
}
