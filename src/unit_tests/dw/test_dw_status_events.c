/** @file test_dw_status_events.c
 *
 *  Standalone unit tests for src/dw/dw_status_events.c:
 *  dw_register_display_status_callback()/dw_unregister_display_status_callback(),
 *  dw_create_display_status_event(), dw_emit_or_queue_display_status_event()
 *  (the queued path, which is deterministic -- no thread is spawned), and
 *  dw_emit_display_status_record() (the direct-emit path, which spawns a
 *  worker thread per registered callback; the test polls with a generous
 *  timeout for the callback to run).
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: dw source files cross-reference each other
 *  and the rest of the ddcutil core extensively, so it links the full
 *  top-level libcommon convenience library (the same aggregate the
 *  ddcutil executable itself links) rather than a minimal per-directory
 *  library set.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "public/ddcutil_types.h"
#include "public/ddcutil_status_codes.h"

#include "base/display_lock.h"
#include "base/displays.h"
#include "base/execution_stats.h"

#include "dw/dw_status_events.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#define CK_INT(expr, expected) do { \
   total++; \
   long _a = (long)(expr); long _e = (long)(expected); \
   if (_a != _e) { failed++; \
      printf("FAIL  line %-4d  %s -> %ld, expected %ld\n", __LINE__, #expr, _a, _e); } \
} while(0)

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)


static void dummy_callback1(DDCA_Display_Status_Event evt) { }
static void dummy_callback2(DDCA_Display_Status_Event evt) { }


static void test_register_unregister_callback(void) {
   // unregistering something never registered: not found
   CK_INT(dw_unregister_display_status_callback(dummy_callback1), DDCRC_NOT_FOUND);

   CK_INT(dw_register_display_status_callback(dummy_callback1), DDCRC_OK);
   CK_INT(dw_register_display_status_callback(dummy_callback1), DDCRC_OK);   // not an error to re-register
   CK_INT(dw_register_display_status_callback(dummy_callback2), DDCRC_OK);

   // dw_unregister_display_status_callback() must report DDCRC_OK (0) when
   // the callback actually was found and removed -- this is the exact
   // condition under which a prior bug returned 1 (looked like failure) --
   // and DDCRC_NOT_FOUND on a second attempt, since it is no longer registered.
   CK_INT(dw_unregister_display_status_callback(dummy_callback1), DDCRC_OK);
   CK_INT(dw_unregister_display_status_callback(dummy_callback1), DDCRC_NOT_FOUND);

   CK_INT(dw_unregister_display_status_callback(dummy_callback2), DDCRC_OK);
}


static void test_dw_create_display_status_event(void) {
   Display_Ref * dref = create_bus_display_ref(201);
   dref->io_path.io_mode = DDCA_IO_I2C;
   dref->io_path.path.i2c_busno = 201;

   DDCA_Display_Status_Event evt =
         dw_create_display_status_event(DDCA_EVENT_DISPLAY_CONNECTED, "card0-DP-1", dref, dref->io_path);

   CK(evt.event_type == DDCA_EVENT_DISPLAY_CONNECTED);
   CK_STR(evt.connector_name, "card0-DP-1");
   CK(evt.io_path.io_mode == DDCA_IO_I2C);
   CK_INT(evt.io_path.path.i2c_busno, 201);
   // DREF_DDC_COMMUNICATION_WORKING not set -> DDCA_DISPLAY_EVENT_DDC_WORKING not set
   CK(!(evt.flags & DDCA_DISPLAY_EVENT_DDC_WORKING));

   dref->flags |= DREF_DDC_COMMUNICATION_WORKING;
   DDCA_Display_Status_Event evt2 =
         dw_create_display_status_event(DDCA_EVENT_DISPLAY_CONNECTED, "card0-DP-1", dref, dref->io_path);
   CK(evt2.flags & DDCA_DISPLAY_EVENT_DDC_WORKING);
}


static void test_dw_emit_or_queue_display_status_event_queued(void) {
   Display_Ref * dref = create_bus_display_ref(202);
   dref->io_path.io_mode = DDCA_IO_I2C;
   dref->io_path.path.i2c_busno = 202;

   GArray * queue = g_array_new(FALSE, FALSE, sizeof(DDCA_Display_Status_Event));

   // a non-NULL queue is appended to, not emitted -- no thread spawned
   dw_emit_or_queue_display_status_event(
         DDCA_EVENT_DISPLAY_DISCONNECTED, "card0-DP-2", dref, dref->io_path, queue);

   CK_INT(queue->len, 1);
   if (queue->len == 1) {
      DDCA_Display_Status_Event evt = g_array_index(queue, DDCA_Display_Status_Event, 0);
      CK(evt.event_type == DDCA_EVENT_DISPLAY_DISCONNECTED);
      CK_STR(evt.connector_name, "card0-DP-2");
   }

   g_array_free(queue, TRUE);
}


static _Atomic(int) callback_invocation_count = 0;
static DDCA_Display_Status_Event captured_event;

static void counting_callback(DDCA_Display_Status_Event evt) {
   captured_event = evt;
   atomic_fetch_add(&callback_invocation_count, 1);
}


static void test_dw_emit_display_status_record_runs_callback(void) {
   callback_invocation_count = 0;
   CK_INT(dw_register_display_status_callback(counting_callback), DDCRC_OK);

   Display_Ref * dref = create_bus_display_ref(203);
   dref->io_path.io_mode = DDCA_IO_I2C;
   dref->io_path.path.i2c_busno = 203;

   // emits directly (queue == NULL): spawns a worker thread per registered
   // callback. Poll with a generous timeout rather than assuming any
   // particular scheduling latency.
   dw_emit_or_queue_display_status_event(
         DDCA_EVENT_DISPLAY_CONNECTED, "card0-DP-3", dref, dref->io_path, NULL);

   bool ran = false;
   for (int i = 0; i < 200 && !ran; i++) {   // up to ~2 seconds
      if (atomic_load(&callback_invocation_count) > 0)
         ran = true;
      else
         g_usleep(10 * 1000);   // 10 ms
   }
   CK(ran);
   if (ran)
      CK_STR(captured_event.connector_name, "card0-DP-3");

   CK_INT(dw_unregister_display_status_callback(counting_callback), DDCRC_OK);
}


int main(int argc, char ** argv) {
   // dw_execute_callback_func() (run in the callback worker thread spawned by
   // dw_emit_display_status_record()) calls unlock_all_displays_for_current_thread(),
   // which requires the display-lock subsystem's lock_records to be allocated.
   init_execution_stats();
   init_i2c_display_lock();

   test_register_unregister_callback();
   test_dw_create_display_status_event();
   test_dw_emit_or_queue_display_status_event_queued();
   test_dw_emit_display_status_record_runs_callback();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
