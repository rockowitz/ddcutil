/** @file test_dsa2.c
 *
 *  Standalone unit tests for the self-contained functions of src/base/dsa2.c
 *  (dynamic sleep algorithm): the enable flag, the multiplier-to-step mapping,
 *  and the tries upper-bound setters' range validation.  The per-bus results
 *  tables depend on detected I2C hardware and are not exercised.
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
#include "base/dsa2.h"

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

static void test_enable(void) {
   bool orig = dsa2_is_enabled();
   dsa2_enable(true);
   CK(dsa2_is_enabled() == true);
   dsa2_enable(false);
   CK(dsa2_is_enabled() == false);
   dsa2_enable(orig);   // restore
}

static void test_multiplier_to_step(void) {
   // step values (multiplier*100): {0,5,10,20,30,50,70,100,130,160,200};
   // returns the index of the first step >= multiplier*100, clamped to the last.
   CK_INT(dsa2_multiplier_to_step(0.0f), 0);
   CK_INT(dsa2_multiplier_to_step(0.03f), 1);   // 3 -> steps[1]=5
   CK_INT(dsa2_multiplier_to_step(0.07f), 2);   // 7 -> steps[2]=10
   CK_INT(dsa2_multiplier_to_step(0.5f), 5);    // 50 -> steps[5]=50
   CK_INT(dsa2_multiplier_to_step(1.0f), 7);    // 100 -> steps[7]=100
   CK_INT(dsa2_multiplier_to_step(2.0f), 10);   // 200 -> steps[10]=200
   CK_INT(dsa2_multiplier_to_step(3.0f), 10);   // above range -> clamped to last
}

static void test_bounds(void) {
   // greatest tries upper bound: valid in 1..MAX_MAX_TRIES (15)
   CK(dsa2_set_greatest_tries_upper_bound(3) == true);
   CK(dsa2_set_greatest_tries_upper_bound(1) == true);
   CK(dsa2_set_greatest_tries_upper_bound(15) == true);
   CK(dsa2_set_greatest_tries_upper_bound(0) == false);
   CK(dsa2_set_greatest_tries_upper_bound(16) == false);

   // average tries upper bound: valid in 1.0..MAX_MAX_TRIES
   CK(dsa2_set_average_tries_upper_bound(1.4) == true);
   CK(dsa2_set_average_tries_upper_bound(1.0) == true);
   CK(dsa2_set_average_tries_upper_bound(0.5) == false);
   CK(dsa2_set_average_tries_upper_bound(20.0) == false);
}

int main(int argc, char ** argv) {
   test_enable();
   test_multiplier_to_step();
   test_bounds();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
