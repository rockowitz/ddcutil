/** @file test_display_retry_data.c
 *
 *  Standalone unit test for src/base/display_retry_data.c.
 *
 *  Most of the module maintains per-display retry statistics keyed on a
 *  Per_Display_Data instance, which requires the per-display-data subsystem to
 *  be initialized.  The one host-independent, self-contained function is
 *  display_index_of_highest_non_zero_counter(), which is exercised here.
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/parms.h"    // MAX_MAX_TRIES

// Defined and exported by display_retry_data.c, not declared in its header.
extern uint16_t display_index_of_highest_non_zero_counter(uint16_t * counters);

static int total = 0;
static int failed = 0;

#define CK_INT(expr, expected) do { \
   total++; \
   long _a = (long)(expr); long _e = (long)(expected); \
   if (_a != _e) { failed++; \
      printf("FAIL  line %-4d  %s -> %ld, expected %ld\n", __LINE__, #expr, _a, _e); } \
} while(0)

int main(int argc, char ** argv) {
   // the counter table is indexed 0 .. MAX_MAX_TRIES+1; the function scans
   // indices MAX_MAX_TRIES+1 down to 2 and returns the highest non-zero one,
   // or 1 if none is set.
   uint16_t counters[MAX_MAX_TRIES + 2];

   memset(counters, 0, sizeof(counters));
   CK_INT(display_index_of_highest_non_zero_counter(counters), 1);   // all zero

   memset(counters, 0, sizeof(counters));
   counters[3] = 5;
   CK_INT(display_index_of_highest_non_zero_counter(counters), 3);

   memset(counters, 0, sizeof(counters));
   counters[3] = 5;
   counters[7] = 2;
   CK_INT(display_index_of_highest_non_zero_counter(counters), 7);   // highest wins

   memset(counters, 0, sizeof(counters));
   counters[MAX_MAX_TRIES + 1] = 1;
   CK_INT(display_index_of_highest_non_zero_counter(counters), MAX_MAX_TRIES + 1);

   memset(counters, 0, sizeof(counters));
   counters[2] = 4;
   CK_INT(display_index_of_highest_non_zero_counter(counters), 2);

   // index 1 is not scanned: a count there alone still yields 1
   memset(counters, 0, sizeof(counters));
   counters[1] = 99;
   CK_INT(display_index_of_highest_non_zero_counter(counters), 1);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
