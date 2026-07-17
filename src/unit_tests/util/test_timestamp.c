/** @file test_timestamp.c
 *
 *  Standalone unit tests for the functions in src/util/timestamp.c.
 *
 *  The formatting functions (formatted_elapsed_time0_t, formatted_time_t,
 *  formatted_epoch_time_t) are deterministic for a given input and are checked
 *  against exact expected output.  The clock functions return current values,
 *  so they are checked against invariants (non-zero, non-decreasing, ordering).
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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util/timestamp.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, _a, _e); } \
} while(0)

static const uint64_t SEC = 1000ULL * 1000 * 1000;   // nanoseconds per second

static void test_formatted_elapsed_time0(void) {
   // seconds field is %3 (right justified, min width 3); fractional field is
   // `precision` digits, zero padded
   CK_STR(formatted_elapsed_time0_t(0, 3),                 "  0.000");
   CK_STR(formatted_elapsed_time0_t(1 * SEC + 500000000, 3), "  1.500");
   CK_STR(formatted_elapsed_time0_t(2345678900ULL, 3),      "  2.345");   // truncates, not rounds
   CK_STR(formatted_elapsed_time0_t(12 * SEC, 3),           " 12.000");
   CK_STR(formatted_elapsed_time0_t(1234 * SEC, 3),        "1234.000");   // width exceeded -> full
   CK_STR(formatted_elapsed_time0_t(999000000ULL, 3),      "  0.999");
   // higher precision
   CK_STR(formatted_elapsed_time0_t(1 * SEC + 500000000, 6), "  1.500000");
   CK_STR(formatted_elapsed_time0_t(1 * SEC + 123456789, 6), "  1.123456");
}

static void test_formatted_time(void) {
   // fixed format SECONDS.MILLISECONDS, seconds %3, millis %03
   CK_STR(formatted_time_t(0),                    "  0.000");
   CK_STR(formatted_time_t(1 * SEC + 500000000),  "  1.500");
   CK_STR(formatted_time_t(2345678900ULL),        "  2.345");
   CK_STR(formatted_time_t(12 * SEC),             " 12.000");
   CK_STR(formatted_time_t(999000000ULL),         "  0.999");
   CK_STR(formatted_time_t(1234 * SEC + 7000000), "1234.007");   // millis zero padded
}

static void test_formatted_epoch_time(void) {
   // force UTC so the localtime-based formatting is deterministic
   setenv("TZ", "UTC0", 1);
   tzset();
   CK_STR(formatted_epoch_time_t((time_t) 0),          "Jan 01 00:00:00");
   CK_STR(formatted_epoch_time_t((time_t) 1000000000), "Sep 09 01:46:40");
   CK_STR(formatted_epoch_time_t((time_t) 1700000000), "Nov 14 22:13:20");
}

static void test_clocks(void) {
   // realtime clock: a plausible wall time (> ~2017-07 in nanoseconds)
   uint64_t rt = cur_realtime_nanosec();
   CK(rt > 1500000000ULL * SEC);

   // monotonic clock: non-decreasing across successive reads
   uint64_t m1 = cur_monotonic_time_nanosec();
   uint64_t m2 = cur_monotonic_time_nanosec();
   CK(m2 >= m1);
   CK(m1 > 0);

   // boot clock includes suspended time, so it is >= the monotonic clock
   // (which shares the same boot origin on Linux); read boot second
   uint64_t mono = cur_monotonic_time_nanosec();
   uint64_t boot = cur_boot_time_nanosec();
   CK(boot >= mono);

   // elapsed time since program start: non-decreasing
   uint64_t e1 = elapsed_time_nanosec();
   uint64_t e2 = elapsed_time_nanosec();
   CK(e2 >= e1);
}

int main(int argc, char ** argv) {
   test_formatted_elapsed_time0();
   test_formatted_time();
   test_formatted_epoch_time();
   test_clocks();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
