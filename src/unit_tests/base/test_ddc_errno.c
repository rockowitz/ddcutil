/** @file test_ddc_errno.c
 *
 *  Standalone unit tests for src/base/ddc_errno.c: looking up DDC status-code
 *  info by number and by name, the formatted description, and the
 *  derived/not-an-error classifiers.
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
#include "base/ddc_errno.h"

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

#define CK_HAS(actual, needle) do { \
   total++; \
   const char * _a = (actual); const char * _n = (needle); \
   if (_a == NULL || strstr(_a, _n) == NULL) { failed++; \
      printf("FAIL  line %-4d  \"%s\" does not contain \"%s\"\n", __LINE__, \
             _a ? _a : "(null)", _n); } \
} while(0)

int main(int argc, char ** argv) {
   // lookup by number: name is the stringified constant, plus a description
   Status_Code_Info * info = ddcrc_find_status_code_info(DDCRC_DDC_DATA);
   CK(info != NULL);
   if (info) {
      CK_STR(info->name, "DDCRC_DDC_DATA");
      CK_STR(info->description, "DDC data error");
   }
   CK(ddcrc_find_status_code_info(999) == NULL);   // not a DDC status code

   // lookup by name
   Status_DDC num = 0;
   CK(ddc_error_name_to_number("DDCRC_RETRIES", &num) == true);
   CK_INT(num, DDCRC_RETRIES);
   CK(ddc_error_name_to_number("DDCRC_NOSUCH", &num) == false);
   CK_INT(num, 0);

   // formatted description
   CK_HAS(ddcrc_desc_t(DDCRC_DDC_DATA), "DDCRC_DDC_DATA");
   CK_HAS(ddcrc_desc_t(DDCRC_DDC_DATA), "DDC data error");
   CK_STR(ddcrc_desc_t(999), "Unexpected status code 999");

   // derived-status-code classifier
   CK(ddcrc_is_derived_status_code(DDCRC_ALL_TRIES_ZERO) == true);
   CK(ddcrc_is_derived_status_code(DDCRC_RETRIES) == true);
   CK(ddcrc_is_derived_status_code(DDCRC_DETERMINED_UNSUPPORTED) == true);
   CK(ddcrc_is_derived_status_code(DDCRC_DDC_DATA) == false);

   // not-an-error classifier
   CK(ddcrc_is_not_error(DDCRC_REPORTED_UNSUPPORTED) == true);
   CK(ddcrc_is_not_error(DDCRC_RETRIES) == false);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
