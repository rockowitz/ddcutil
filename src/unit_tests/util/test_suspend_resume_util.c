/** @file test_suspend_resume_util.c
 *
 *  Standalone unit tests for host-independent functions in
 *  src/util/suspend_resume_util.c.
 *
 *  millisec_since_resume_detected_by_clocktime() returns UINT64_MAX until a
 *  resume is detected, which is the state at program start.
 *  recently_resumed_from_sleep() is driven through the logind sleep timestamps
 *  that dbus_util.c records, which needs no bus.  The clocktime detector
 *  proper cannot be exercised here: it reports a resume only after more than a
 *  second of accumulated sleep, and a test run accumulates none.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  Not a libddcutil client -- it links the internal util convenience library
 *  directly, since these symbols are not exported from libddcutil.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/suspend_resume_util.h"
#ifdef USE_DBUS
#include <stdatomic.h>
#include "util/dbus_util.h"
#include "util/timestamp.h"

// Internal symbols of dbus_util.c, non-static but not declared in its header.
extern _Atomic uint64_t last_resume_from_sleep_ns;
extern _Atomic uint64_t last_prepare_for_sleep_ns;
extern _Atomic uint64_t retired_prepare_for_sleep_ns;
#endif

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#ifdef USE_DBUS
// recently_resumed_from_sleep() reports a resume for the duration of an open
// sleep cycle, i.e. a PrepareForSleep(true) whose PrepareForSleep(false) has
// not arrived.  Driving it through the two timestamps needs no bus, and the
// clocktime detector reports nothing here: no sleep accumulates during a test
// run, so what the checks below see is the sleep-cycle rule alone.
static void test_open_sleep_cycle(void) {
   uint64_t ms = 0;
   Resume_Detection detection = RESUME_DETECTED_NONE;

   // no cycle has been retired.  Which cycles the sleep watch thread retires
   // is checked in test_dbus_util.
   retired_prepare_for_sleep_ns = 0;

   // no cycle open, last resume long past: not a recent resume
   last_resume_from_sleep_ns = 1;                  // ~boot, long ago
   last_prepare_for_sleep_ns = 0;
   CK(recently_resumed_from_sleep(500, &ms, &detection) == false);
   CK(ms == UINT64_MAX);
   CK(detection == RESUME_DETECTED_NONE);

   // an open cycle is reported as a resume that just occurred, whatever the
   // stale resume timestamp says
   last_prepare_for_sleep_ns = cur_boot_time_nanosec();
   CK(recently_resumed_from_sleep(500, &ms, &detection) == true);
   CK(ms == 0);
   // reported as a sleep cycle, not as a resume: the system need not have
   // slept yet, and dw_pause_if_recently_resumed_from_sleep() says so in the
   // system log
   CK(detection == RESUME_DETECTED_IN_SLEEP_CYCLE);

   // within_ms 0 asks whether a resume occurred within no time at all
   CK(recently_resumed_from_sleep(0, &ms, NULL) == false);

   // closing the cycle hands the answer back to the resume timestamp, which
   // is now current
   last_resume_from_sleep_ns = cur_boot_time_nanosec();
   CK(recently_resumed_from_sleep(500, &ms, &detection) == true);
   CK(ms < 500);
   CK(detection == RESUME_DETECTED_BY_DBUS);

   // and once that timestamp is old, no resume is reported
   last_resume_from_sleep_ns = 1;
   last_prepare_for_sleep_ns = 0;
   CK(recently_resumed_from_sleep(500, &ms, NULL) == false);

   // a retired cycle is not a resume either: the prepare signal is still
   // unmatched, but the thread has concluded its counterpart is not coming
   last_prepare_for_sleep_ns    = cur_boot_time_nanosec();
   retired_prepare_for_sleep_ns = last_prepare_for_sleep_ns;
   CK(recently_resumed_from_sleep(500, &ms, NULL) == false);
}
#endif

// Every detection value has a description, and none falls through to the
// "unrecognized" default.
static void test_detection_descriptions(void) {
   const Resume_Detection all[] = {RESUME_DETECTED_NONE,
                                   RESUME_DETECTED_BY_DBUS,
                                   RESUME_DETECTED_BY_CLOCKTIME,
                                   RESUME_DETECTED_IN_SLEEP_CYCLE};
   for (int ndx = 0; ndx < 4; ndx++) {
      const char * s = resume_detection_description(all[ndx]);
      CK(s != NULL);
      CK(s[0] != '\0');
      CK(strcmp(s, "Unrecognized resume detection") != 0);
   }
}

int main(int argc, char ** argv) {
   // no resume has been detected at program start
   CK(millisec_since_resume_detected_by_clocktime() == UINT64_MAX);

   // an observer must not open the grace window: no sleep has accumulated, so
   // it reports no resume, and the detector's state is unchanged either way
   CK(recently_resumed_from_sleep_by_clocktime0(/*no_mutate=*/true, NULL) == false);
   CK(millisec_since_resume_detected_by_clocktime() == UINT64_MAX);

   test_detection_descriptions();

#ifdef USE_DBUS
   test_open_sleep_cycle();
#endif

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
