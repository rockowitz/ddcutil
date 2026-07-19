/** @file test_status_code_mgt.c
 *
 *  Standalone unit tests for src/base/status_code_mgt.c: determining the
 *  modulation range of a status code, the ADL modulate/demodulate round trip,
 *  and the status-code name lookup.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libbase unit test: it links the internal libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "public/ddcutil_status_codes.h"
#include "base/ddc_errno.h"
#include "base/linux_errno.h"
#include "base/status_code_mgt.h"

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

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)

int main(int argc, char ** argv) {
   init_status_code_mgt();
   init_linux_errno();

   // get_modulation classifies a status code into its range
   CK_INT(get_modulation(-EBUSY), RR_ERRNO);          // small negative -> errno range
   CK_INT(get_modulation(DDCRC_DDC_DATA), RR_DDC);    // ddcutil-specific range

   // ADL modulate / demodulate round trip
   CK_INT(demodulate_rc(modulate_rc(-3, RR_ADL), RR_ADL), -3);
   CK_INT(demodulate_rc(modulate_rc(5, RR_ADL), RR_ADL), 5);
   CK_INT(modulate_rc(0, RR_ADL), 0);                 // zero is unchanged

   // status code name lookup
   CK_STR(psc_name(DDCRC_DDC_DATA), "DDCRC_DDC_DATA");
   CK_STR(psc_name(-EBUSY), "EBUSY");

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
