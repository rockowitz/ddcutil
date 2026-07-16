/** @file test_sysfs_filter_functions.c
 *
 *  Standalone unit tests for src/util/sysfs_filter_functions.c: the name-based
 *  predicate functions (cardN, cardN-connector, i2c-N, and the D-00hh device
 *  address form) and the simple filename comparators.  These are pure functions
 *  of their string arguments; the predicates that stat /sys are not exercised.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/sysfs_filter_functions.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

static void test_cardN(void) {
   CK(predicate_cardN("card0") == true);
   CK(predicate_cardN("card12") == true);
   CK(predicate_cardN("card") == false);       // no digits
   CK(predicate_cardN("cardX") == false);
   CK(predicate_cardN("xcard0") == false);      // must be anchored at start
   CK(predicate_cardN("card0-DP-1") == false);  // must be anchored at end
}

static void test_cardN_connector(void) {
   CK(predicate_cardN_connector("card0-DP-1") == true);
   CK(predicate_cardN_connector("card1-") == true);       // needs the trailing hyphen
   CK(predicate_cardN_connector("card0") == false);        // no hyphen
   CK(predicate_cardN_connector("card") == false);
}

static void test_i2c_N(void) {
   CK(predicate_i2c_N("i2c-3") == true);
   CK(predicate_i2c_N("i2c-0") == true);
   CK(predicate_i2c_N("i2c-") == false);        // no number
   CK(predicate_i2c_N("i2c-3x") == false);
   CK(predicate_i2c_N("xi2c-3") == false);
}

static void test_D_00hh(void) {
   CK(predicate_any_D_00hh("3-0037") == true);
   CK(predicate_any_D_00hh("10-00ab") == true);   // hex digits allowed
   CK(predicate_any_D_00hh("3-0037x") == false);
   CK(predicate_any_D_00hh("3-37") == false);      // needs the "00" prefix
   CK(predicate_any_D_00hh("abc-0037") == false);

   // exact form also checks the bus-number prefix
   CK(predicate_exact_D_00hh("3-0037", "3") == true);
   CK(predicate_exact_D_00hh("3-0037", "5") == false);
   CK(predicate_exact_D_00hh("not-a-match", "3") == false);
}

static void test_fn_comparators(void) {
   CK(fn_equal("abc", "abc") == true);
   CK(fn_equal("abc", "abd") == false);
   CK(fn_starts_with("abcdef", "abc") == true);
   CK(fn_starts_with("abcdef", "xyz") == false);
   CK(fn_starts_with("abc", "abcdef") == false);   // prefix longer than string
}

int main(int argc, char ** argv) {
   test_cardN();
   test_cardN_connector();
   test_i2c_N();
   test_D_00hh();
   test_fn_comparators();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
