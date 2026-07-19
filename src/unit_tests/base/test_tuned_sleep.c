/** @file test_tuned_sleep.c
 *
 *  Standalone unit test for the deferred-sleep flag in src/base/tuned_sleep.c.
 *  The actual timed sleeps depend on per-display state and are not exercised.
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

#include "base/tuned_sleep.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

int main(int argc, char ** argv) {
   bool orig = is_deferred_sleep_enabled();

   // enable_deferred_sleep returns the previous value; the getter reflects it
   enable_deferred_sleep(true);
   CK(is_deferred_sleep_enabled() == true);
   CK(enable_deferred_sleep(false) == true);    // returns previous (true)
   CK(is_deferred_sleep_enabled() == false);
   CK(enable_deferred_sleep(true) == false);    // returns previous (false)
   CK(is_deferred_sleep_enabled() == true);

   enable_deferred_sleep(orig);                 // restore

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
