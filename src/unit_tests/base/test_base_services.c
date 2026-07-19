/** @file test_base_services.c
 *
 *  Standalone unit test for src/base/base_services.c: init_base_services()
 *  initializes the base subsystems.  This is verified as a smoke test and by
 *  confirming that services it initializes (execution stats, sleep stats) are
 *  usable afterward without any additional explicit init.
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

#include "public/ddcutil_status_codes.h"
#include "base/execution_stats.h"
#include "base/sleep.h"
#include "base/base_services.h"

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
   init_base_services();

   // execution stats were initialized: log_status_code works and returns the code
   CK_INT(log_status_code(DDCRC_DDC_DATA, "test"), DDCRC_DDC_DATA);

   // sleep stats were initialized (reset to zero)
   Sleep_Stats s = get_sleep_stats();
   CK_INT(s.total_sleep_calls, 0);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
