/** @file test_x11_util.c
 *
 *  Standalone unit tests for src/util/x11_util.c.
 *
 *  dpms_power_level_name() maps a DPMS power level to its name; it is a pure
 *  function and is checked here.  get_x11_edids() and get_x11_dpms_info()
 *  require a running X server and are not exercised.
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

#include "util/x11_util.h"

static int total = 0;
static int failed = 0;

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)

int main(int argc, char ** argv) {
   CK_STR(dpms_power_level_name(0), "DPMSModeOn");
   CK_STR(dpms_power_level_name(1), "DPMSModeStandby");
   CK_STR(dpms_power_level_name(2), "DPMSModeSuspend");
   CK_STR(dpms_power_level_name(3), "DPMSModeOff");
   CK_STR(dpms_power_level_name(99), "Invalid Value");

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
