/** @file test_per_display_data.c
 *
 *  Standalone unit tests for the self-contained functions of
 *  src/base/per_display_data.c: the user-multiplier source name, the dynamic
 *  sleep enabled flag (which reflects the dsa2 setting), and the default sleep
 *  multiplier get/set.  The per-display record tables require the subsystem to
 *  be initialized and are not exercised here.
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
#include "base/per_display_data.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)

static void test_source_name(void) {
   CK_STR(user_multiplier_source_name(Default),  "Implicit");
   CK_STR(user_multiplier_source_name(Explicit), "Explicit");
   CK_STR(user_multiplier_source_name(Reset),    "Reset");
}

static void test_dynamic_sleep_flag(void) {
   bool orig = dsa2_is_enabled();
   dsa2_enable(true);
   CK(pdd_is_dynamic_sleep_enabled() == true);    // reflects the dsa2 flag
   dsa2_enable(false);
   CK(pdd_is_dynamic_sleep_enabled() == false);
   dsa2_enable(orig);
}

static void test_default_multiplier(void) {
   DDCA_Sleep_Multiplier orig = pdd_get_default_sleep_multiplier_factor();

   pdd_set_default_sleep_multiplier_factor(2.0, Explicit);
   CK(pdd_get_default_sleep_multiplier_factor() == 2.0);
   pdd_set_default_sleep_multiplier_factor(0.5, Explicit);
   CK(pdd_get_default_sleep_multiplier_factor() == 0.5);

   pdd_set_default_sleep_multiplier_factor(orig, Reset);   // restore
}

int main(int argc, char ** argv) {
   test_source_name();
   test_dynamic_sleep_flag();
   test_default_multiplier();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
