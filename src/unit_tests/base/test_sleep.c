/** @file test_sleep.c
 *
 *  Standalone unit tests for src/base/sleep.c: the sleep-statistics counters
 *  updated by loggable_sleep() (with SLEEP_OPT_STATS) and read via
 *  get_sleep_stats().
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libbase unit test: it links the internal libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "public/ddcutil_types.h"
#include "base/sleep.h"

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

int main(int argc, char ** argv) {
   init_sleep_stats();
   Sleep_Stats s = get_sleep_stats();
   CK_INT(s.total_sleep_calls, 0);
   CK_INT(s.requested_sleep_milliseconds, 0);
   CK_INT(s.actual_sleep_nanos, 0);

   // loggable_sleep with SLEEP_OPT_STATS records the sleep in the counters
   loggable_sleep(1, SLEEP_OPT_STATS, DDCA_SYSLOG_NEVER, "test", __LINE__, __FILE__, NULL);
   s = get_sleep_stats();
   CK_INT(s.total_sleep_calls, 1);
   CK_INT(s.requested_sleep_milliseconds, 1);
   CK(s.actual_sleep_nanos > 0);         // some real time elapsed

   loggable_sleep(2, SLEEP_OPT_STATS, DDCA_SYSLOG_NEVER, "test", __LINE__, __FILE__, NULL);
   s = get_sleep_stats();
   CK_INT(s.total_sleep_calls, 2);
   CK_INT(s.requested_sleep_milliseconds, 3);   // 1 + 2 accumulated

   // reset clears the counters
   init_sleep_stats();
   s = get_sleep_stats();
   CK_INT(s.total_sleep_calls, 0);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
