/** @file stress_watch.c
 *
 *  Concurrency stress harness for libddcutil.
 *
 *  Exercises the display-watch, recheck, quiesce/redetect, and callback
 *  machinery together with concurrent API calls, to shake out locking
 *  defects in the shared data structures (all_display_refs, all_i2c_buses,
 *  the display locks, the per-thread/per-display data, the try-data stats)
 *  and in the quiesce path used by ddca_redetect_displays().
 *
 *  It cannot inject synthetic hotplug or suspend/resume events; it drives
 *  the real code paths reachable without hardware events, under many
 *  threads at once. Run it with a real display attached, ideally on an
 *  ASan build (./configure --enable-asan), and while manually plugging /
 *  unplugging a monitor and suspending / resuming the machine to add the
 *  hardware-event dimension.
 *
 *  Build: added as a sample client; produces the `stress_watch` binary.
 *  Usage: stress_watch [seconds] [n_worker_threads] [redetect_period_ms]
 *         defaults: 20 seconds, 8 threads, redetect every 2000 ms
 *
 *  Exit status: 0 if no failure detected, 1 otherwise. A crash, hang, or
 *  ASan/assert abort is itself the finding.
 *
 *  Op selection: by default the workers run display-ref enumeration and display
 *  open/getvcp/close (ops 0 and 3), which are safe to run concurrently with the
 *  redetect driver -- the combination this harness primarily validates. Pass a
 *  4th argument to force a single op index (0..4): 0 = get_display_refs,
 *  1 = get_display_info_list2, 2 = report_displays, 3 = open/getvcp/close,
 *  4 = yield.
 *
 *  ops 1 and 2 are NOT in the default mix. They originally corrupted the heap
 *  under concurrency via two unsynchronized global hash tables (per_thread_data.c
 *  and pnp_ids.c); both were found by this harness and fixed, and ops 1 and 2 are
 *  now clean under concurrency on their own (verified under ASan, e.g.
 *  `stress_watch 10 16 999999 2`). They remain excluded from the default mix
 *  because concurrent report_displays / get_display_info_list2 while the redetect
 *  driver is active still crashes: redetect frees the Display_Refs, and
 *  ddca_report_displays() in particular does not respect the API quiesce, so it
 *  dereferences freed drefs (ASan: SEGV in record_i2c_edid_use). That is a
 *  separate, still-open issue; run ops 1/2 with redetect disabled (large 3rd arg)
 *  to exercise them without it.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "public/ddcutil_c_api.h"
#include "public/ddcutil_status_codes.h"
#include "public/ddcutil_types.h"

static int          forced_op       = -1;    // -1 = random mix; else pin to one case
static atomic_bool  stop            = false;
static atomic_long  op_count        = 0;
static atomic_long  event_count     = 0;
static atomic_long  redetect_count  = 0;
static atomic_long  open_fail_count = 0;   // non-fatal: display may be busy/removed

// A registered callback runs on a libddcutil callback thread.  Keep it
// trivial and thread-safe; its mere invocation exercises the callback
// snapshot/dispatch path.
static void status_callback(DDCA_Display_Status_Event event) {
   atomic_fetch_add(&event_count, 1);
}

static uint64_t now_millis(void) {
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (uint64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// A worker cycles through the read/enumerate/open API surface.  None of
// these should ever crash or deadlock regardless of interleaving with the
// watch thread's hotplug processing or another worker's redetect.
static void * worker(void * arg) {
   unsigned int seed = (unsigned int)(uintptr_t) arg ^ (unsigned int) now_millis();
   // Each thread suppresses its own normal output so report calls don't
   // interleave into unreadable noise; the report path locking is still
   // exercised.
   ddca_set_fout(NULL);

   // Default mix = ops safe to run alongside the redetect driver.  ops 1 and 2
   // are reachable only via forced_op (see file header).
   static const int default_ops[] = {0, 3, 3, 4};
   while (!atomic_load(&stop)) {
      int which = (forced_op >= 0)
                     ? forced_op
                     : default_ops[rand_r(&seed) % (int)(sizeof(default_ops)/sizeof(default_ops[0]))];
      switch (which) {

      case 0: {   // enumerate display refs (reads all_display_refs)
         DDCA_Display_Ref * drefs = NULL;
         if (ddca_get_display_refs(true, &drefs) == 0 && drefs)
            free(drefs);
         break;
      }

      case 1: {   // enumerate display info (reads all_display_refs + businfo)
         DDCA_Display_Info_List * dlist = NULL;
         if (ddca_get_display_info_list2(true, &dlist) == 0 && dlist)
            ddca_free_display_info_list(dlist);
         break;
      }

      case 2: {   // report displays (snapshot-under-lock path)
         ddca_report_displays(true, 0);
         break;
      }

      case 3: {   // open a display, read brightness, close
         DDCA_Display_Ref * drefs = NULL;
         if (ddca_get_display_refs(false, &drefs) == 0 && drefs && drefs[0]) {
            // pick a pseudo-random dref from the null-terminated array
            int n = 0;
            while (drefs[n]) n++;
            DDCA_Display_Ref dref = drefs[rand_r(&seed) % n];
            DDCA_Display_Handle dh = NULL;
            if (ddca_open_display2(dref, false, &dh) == 0 && dh) {
               DDCA_Non_Table_Vcp_Value valrec;
               ddca_get_non_table_vcp_value(dh, 0x10, &valrec);  // brightness
               ddca_close_display(dh);
            }
            else {
               atomic_fetch_add(&open_fail_count, 1);
            }
            free(drefs);
         }
         break;
      }

      case 4:     // brief yield to widen interleavings
         usleep(rand_r(&seed) % 2000);
         break;
      }
      atomic_fetch_add(&op_count, 1);
   }
   return NULL;
}

// The redetect driver periodically calls ddca_redetect_displays(), the
// heaviest path: it quiesces the API, discards and rebuilds all_display_refs
// and all_i2c_buses, and resets the display-lock table, all while the
// workers keep calling in.  This is the primary target of the harness.
static void * redetect_driver(void * arg) {
   int period_ms = (int)(intptr_t) arg;
   ddca_set_fout(NULL);
   while (!atomic_load(&stop)) {
      for (int slept = 0; slept < period_ms && !atomic_load(&stop); slept += 50)
         usleep(50 * 1000);
      if (atomic_load(&stop))
         break;
      DDCA_Status rc = ddca_redetect_displays();
      atomic_fetch_add(&redetect_count, 1);
      if (rc != 0 && rc != DDCRC_INVALID_OPERATION) {
         // DDCRC_INVALID_OPERATION just means a redetect was already active;
         // anything else is worth noting but not necessarily a failure.
         fprintf(stderr, "[redetect] ddca_redetect_displays() returned %d (%s)\n",
                 rc, ddca_rc_name(rc));
      }
   }
   return NULL;
}

int main(int argc, char ** argv) {
   int   seconds       = (argc > 1) ? atoi(argv[1]) : 20;
   int   n_workers     = (argc > 2) ? atoi(argv[2]) : 8;
   int   redetect_ms   = (argc > 3) ? atoi(argv[3]) : 2000;
   forced_op           = (argc > 4) ? atoi(argv[4]) : -1;   // pin worker op for bisection
   if (n_workers < 1)  n_workers = 1;
   if (seconds   < 1)  seconds   = 1;

   printf("stress_watch: %d seconds, %d worker threads, redetect every %d ms\n",
          seconds, n_workers, redetect_ms);

   char ** infomsgs = NULL;
   DDCA_Status rc = ddca_init2("", DDCA_SYSLOG_ERROR, DDCA_INIT_OPTIONS_NONE, &infomsgs);
   if (rc != 0) {
      fprintf(stderr, "ddca_init2() failed: %d (%s)\n", rc, ddca_rc_name(rc));
      return 1;
   }

   rc = ddca_register_display_status_callback(status_callback);
   if (rc != 0)
      fprintf(stderr, "warning: ddca_register_display_status_callback() returned %d\n", rc);

   rc = ddca_start_watch_displays(DDCA_EVENT_CLASS_ALL);
   if (rc != 0)
      fprintf(stderr, "warning: ddca_start_watch_displays() returned %d (%s)\n",
              rc, ddca_rc_name(rc));

   pthread_t redetect_thread;
   pthread_t * workers = calloc(n_workers, sizeof(pthread_t));
   for (int i = 0; i < n_workers; i++)
      pthread_create(&workers[i], NULL, worker, (void *)(uintptr_t) i);
   pthread_create(&redetect_thread, NULL, redetect_driver, (void *)(intptr_t) redetect_ms);

   // progress heartbeat, so a hang is visible as a stalled op counter
   for (int elapsed = 0; elapsed < seconds; elapsed++) {
      sleep(1);
      printf("  t=%2ds  ops=%-9ld events=%-6ld redetects=%-4ld open_fails=%ld\n",
             elapsed + 1,
             atomic_load(&op_count), atomic_load(&event_count),
             atomic_load(&redetect_count), atomic_load(&open_fail_count));
   }

   atomic_store(&stop, true);
   fprintf(stderr, "[shutdown] joining workers...\n");
   for (int i = 0; i < n_workers; i++)
      pthread_join(workers[i], NULL);
   fprintf(stderr, "[shutdown] joining redetect driver...\n");
   pthread_join(redetect_thread, NULL);
   free(workers);

   fprintf(stderr, "[shutdown] stopping watch...\n");
   ddca_stop_watch_displays(true);
   fprintf(stderr, "[shutdown] unregistering callback...\n");
   ddca_unregister_display_status_callback(status_callback);
   fprintf(stderr, "[shutdown] done\n");

   ddca_set_fout_to_default();
   printf("stress_watch: done. total ops=%ld, events=%ld, redetects=%ld, open_fails=%ld\n",
          atomic_load(&op_count), atomic_load(&event_count),
          atomic_load(&redetect_count), atomic_load(&open_fail_count));
   printf("No crash, hang, or abort detected.\n");
   return 0;
}
