/** @file test_vcp_version.c
 *
 *  Standalone unit tests for src/base/vcp_version.c: parsing and formatting an
 *  MCCS version spec, the version comparison predicates, and validity checking.
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
#include "base/vcp_version.h"

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

// mutable buffers for parse_vspec (takes char*)
static DDCA_MCCS_Version_Spec parse(const char * s) {
   char buf[32];
   g_strlcpy(buf, s, sizeof(buf));
   return parse_vspec(buf);
}

static void test_parse(void) {
   CK(vcp_version_eq(parse("1.0"), DDCA_VSPEC_V10));
   CK(vcp_version_eq(parse("2.1"), DDCA_VSPEC_V21));
   CK(vcp_version_eq(parse("2.2"), DDCA_VSPEC_V22));
   CK(vcp_version_eq(parse("3.0"), DDCA_VSPEC_V30));
   CK(vcp_version_eq(parse("9.9"), DDCA_VSPEC_UNKNOWN));   // major out of range
   CK(vcp_version_eq(parse("garbage"), DDCA_VSPEC_UNKNOWN));
}

static void test_format(void) {
   CK_STR(format_vspec(DDCA_VSPEC_V21), "2.1");
   CK_STR(format_vspec(DDCA_VSPEC_V30), "3.0");
   CK_STR(format_vspec(DDCA_VSPEC_UNKNOWN), "Unknown");
   CK_STR(format_vspec(DDCA_VSPEC_UNQUERIED), "Unqueried");
}

static void test_compare(void) {
   CK(vcp_version_eq(DDCA_VSPEC_V21, DDCA_VSPEC_V21) == true);
   CK(vcp_version_eq(DDCA_VSPEC_V21, DDCA_VSPEC_V30) == false);

   CK(vcp_version_le(DDCA_VSPEC_V20, DDCA_VSPEC_V21) == true);
   CK(vcp_version_le(DDCA_VSPEC_V21, DDCA_VSPEC_V21) == true);
   CK(vcp_version_le(DDCA_VSPEC_V21, DDCA_VSPEC_V20) == false);

   CK(vcp_version_lt(DDCA_VSPEC_V20, DDCA_VSPEC_V21) == true);
   CK(vcp_version_lt(DDCA_VSPEC_V21, DDCA_VSPEC_V21) == false);

   CK(vcp_version_gt(DDCA_VSPEC_V30, DDCA_VSPEC_V21) == true);
   CK(vcp_version_gt(DDCA_VSPEC_V21, DDCA_VSPEC_V30) == false);
}

static void test_is_valid(void) {
   CK(vcp_version_is_valid(DDCA_VSPEC_V21, false) == true);
   CK(vcp_version_is_valid(DDCA_VSPEC_V10, false) == true);
   CK(vcp_version_is_valid(DDCA_VSPEC_UNKNOWN, false) == false);
   CK(vcp_version_is_valid(DDCA_VSPEC_UNKNOWN, true) == true);   // allow_unknown

   DDCA_MCCS_Version_Spec bogus = {9, 9};
   CK(vcp_version_is_valid(bogus, false) == false);
}

int main(int argc, char ** argv) {
   test_parse();
   test_format();
   test_compare();
   test_is_valid();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
