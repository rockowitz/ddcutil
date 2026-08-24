/** @file test_dbus_util.c
 *
 *  Standalone unit tests for the bus-independent parts of src/util/dbus_util.c.
 *
 *  The sleep-watch thread and D-Bus signal handling require a live system bus
 *  and cannot be exercised here.  What can be tested deterministically is the
 *  prepare-for-sleep callback registry (register / unregister / invoke) and the
 *  sleep-cycle timestamp bookkeeping used by ldbus_elapsed_since_resume_from_sleep_ns()
 *  and ldbus_in_open_sleep_cycle().
 *  Those touch internal, non-static symbols that are not in the public header,
 *  declared below.
 *
 *  The checks of ldbus_pause_if_recent_return_from_sleep() are #ifdef UNUSED,
 *  as is the function itself.  It was superseded by
 *  recently_resumed_from_sleep() in linux_util.c.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  Not a libddcutil client -- it links the internal util convenience library
 *  directly, since these symbols are not exported from libddcutil.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "util/dbus_util.h"
#include "util/timestamp.h"

// Internal symbols of dbus_util.c, non-static but not declared in the header.
extern void invoke_prepare_for_sleep_callbacks(bool preparing);
extern void ldbus_elapsed_since_resume_from_sleep_mark_start(void);
extern _Atomic uint64_t last_resume_from_sleep_ns;
extern _Atomic uint64_t last_prepare_for_sleep_ns;
extern _Atomic uint64_t retired_prepare_for_sleep_ns;

#define SECOND_NS (1000ULL * 1000 * 1000)

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

static int  cb1_count = 0;
static int  cb2_count = 0;
static bool cb1_last  = false;
static void cb1(bool preparing) { cb1_count++; cb1_last = preparing; }
static void cb2(bool preparing) { cb2_count++; }

static void test_callbacks(void) {
   ldbus_register_prepare_for_sleep_callback(cb1);
   ldbus_register_prepare_for_sleep_callback(cb2);

   invoke_prepare_for_sleep_callbacks(true);
   CK_INT(cb1_count, 1);
   CK_INT(cb2_count, 1);
   CK(cb1_last == true);

   invoke_prepare_for_sleep_callbacks(false);
   CK_INT(cb1_count, 2);
   CK_INT(cb2_count, 2);
   CK(cb1_last == false);

   // after unregistering cb1, only cb2 fires
   ldbus_unregister_prepare_for_sleep_callback(cb1);
   invoke_prepare_for_sleep_callbacks(true);
   CK_INT(cb1_count, 2);      // unchanged
   CK_INT(cb2_count, 3);

   // after unregistering cb2 as well, nothing fires
   ldbus_unregister_prepare_for_sleep_callback(cb2);
   invoke_prepare_for_sleep_callbacks(true);
   CK_INT(cb1_count, 2);
   CK_INT(cb2_count, 3);
}

static void test_resume_timing(void) {
   // marking start makes "now" the resume reference, so elapsed is tiny
   ldbus_elapsed_since_resume_from_sleep_mark_start();
   uint64_t elapsed = ldbus_elapsed_since_resume_from_sleep_ns();
   CK(elapsed < 1000ULL * 1000 * 1000);        // < 1 second

#ifdef UNUSED
   // a just-marked resume with a zero minimum requires no pause
   CK_INT(ldbus_pause_if_recent_return_from_sleep(0), 0);

   // a just-marked resume with a positive minimum pauses for ~the whole minimum
   ldbus_elapsed_since_resume_from_sleep_mark_start();
   int slept = ldbus_pause_if_recent_return_from_sleep(40);
   CK(slept > 0 && slept <= 40);

   // a resume far in the past requires no pause
   last_resume_from_sleep_ns = 0;              // epoch of the boot clock, long ago
   CK_INT(ldbus_pause_if_recent_return_from_sleep(500), 0);
#endif
}

// The two timestamps recorded from PrepareForSleep bracket a sleep cycle: it
// is open when the prepare is the later of the two, and stays open until the
// resume signal closes it or the sleep watch thread retires it.  The signals
// themselves need a live bus, but the bookkeeping they drive does not.
//
// Only the ordering of the three timestamps matters to the predicate, so the
// states below are built from small boot-clock values rather than offsets from
// the current time, which would underflow on a host of short uptime.
static void test_open_sleep_cycle(void) {
   uint64_t elapsed = 0;

   // program start is not an open cycle: mark_start sets both timestamps alike
   ldbus_elapsed_since_resume_from_sleep_mark_start();
   retired_prepare_for_sleep_ns = 0;
   CK(ldbus_in_open_sleep_cycle(&elapsed) == false);
   CK(elapsed == UINT64_MAX);

   // PrepareForSleep(true) opens a cycle, elapsed measured from the signal
   last_prepare_for_sleep_ns = cur_boot_time_nanosec();
   CK(ldbus_in_open_sleep_cycle(&elapsed) == true);
   CK(elapsed != UINT64_MAX);
   CK(elapsed < SECOND_NS);

   // the cycle stays open however long the suspend lasts, and the elapsed time
   // it reports spans that suspend.  A resume recorded before the prepare --
   // an earlier cycle's, or the mark at program start -- does not close it.
   last_resume_from_sleep_ns = 1;             // ~boot, long ago
   last_prepare_for_sleep_ns = 2;
   CK(ldbus_in_open_sleep_cycle(&elapsed) == true);
   CK(elapsed > SECOND_NS);                   // i.e. the whole uptime

   // retiring the cycle leaves the prepare signal unmatched, and so still
   // reported, but no longer treated as a resume
   retired_prepare_for_sleep_ns = last_prepare_for_sleep_ns;
   CK(ldbus_in_open_sleep_cycle(&elapsed) == false);
   CK(elapsed != UINT64_MAX);

   // retirement applies to that cycle alone: the next prepare opens a new one
   last_prepare_for_sleep_ns = 3;
   CK(ldbus_in_open_sleep_cycle(&elapsed) == true);

   // the matching PrepareForSleep(false) closes it
   last_resume_from_sleep_ns = cur_boot_time_nanosec();
   CK(ldbus_in_open_sleep_cycle(&elapsed) == false);
   CK(elapsed == UINT64_MAX);

   // equal timestamps are not an open cycle
   last_prepare_for_sleep_ns = last_resume_from_sleep_ns;
   CK(ldbus_in_open_sleep_cycle(&elapsed) == false);
}

int main(int argc, char ** argv) {
   test_callbacks();
   test_resume_timing();
   test_open_sleep_cycle();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
