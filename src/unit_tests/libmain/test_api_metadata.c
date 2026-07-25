/** @file test_api_metadata.c
 *
 *  Standalone unit tests for src/libmain/api_metadata.c: the
 *  DDCA_Feature_List bitset wrappers (clear/add/contains/eq/or/and/
 *  and_not/count/string), ddca_feature_list_id_name(), and
 *  ddca_get_feature_name() (the by-id-only overload).
 *
 *  These wrap functions in base/feature_lists.c and vcp/vcp_feature_codes.c
 *  that already have dedicated unit tests (test_feature_lists.c,
 *  test_vcp_feature_codes.c); the point of testing them again here is the
 *  public API wrapper layer itself -- confirming argument order and
 *  pass-through are correct at the actual documented entry point, not
 *  re-verifying the underlying bitset algorithm.
 *
 *  Every other function in api_metadata.c either lives inside an #ifdef
 *  UNUSED/NEVER_RELEASED/DEPRECATED block (not compiled) or is wrapped in
 *  the API_PROLOG/API_PROLOGX macros, which perform full implicit library
 *  initialization (including real I2C bus/display detection) the first
 *  time any such function is called if the library isn't already
 *  initialized -- exactly the kind of real-hardware side effect these
 *  unit tests are designed to avoid.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon+libsharedlib unit test: it links the internal
 *  libmain/libsharedlib.la convenience library (the intermediate library
 *  that becomes libddcutil.so) together with the top-level libcommon
 *  convenience library it depends on.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "public/ddcutil_c_api.h"
#include "public/ddcutil_types.h"

#include "vcp/vcp_feature_codes.h"

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

#define CK_STR_CONTAINS(actual, needle) do { \
   total++; \
   const char * _a = (actual); const char * _n = (needle); \
   if (_a == NULL || strstr(_a, _n) == NULL) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected to contain \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _n); } \
} while(0)


static void test_feature_list_clear_add_contains(void) {
   DDCA_Feature_List list = DDCA_EMPTY_FEATURE_LIST;
   CK(!ddca_feature_list_contains(list, 0x10));

   list = ddca_feature_list_add(&list, 0x10);
   CK(ddca_feature_list_contains(list, 0x10));
   CK(!ddca_feature_list_contains(list, 0x12));

   ddca_feature_list_clear(&list);
   CK(!ddca_feature_list_contains(list, 0x10));
}


static void test_feature_list_id_name(void) {
   CK_STR(ddca_feature_list_id_name(DDCA_SUBSET_KNOWN),   "DDCA_SUBSET_KNOWN");
   CK_STR(ddca_feature_list_id_name(DDCA_SUBSET_COLOR),   "DDCA_SUBSET_COLOR");
   CK_STR(ddca_feature_list_id_name(DDCA_SUBSET_PROFILE), "DDCA_SUBSET_PROFILE");
   CK_STR(ddca_feature_list_id_name(DDCA_SUBSET_MFG),     "DDCA_SUBSET_MFG");
   CK_STR(ddca_feature_list_id_name(DDCA_SUBSET_UNSET),   "DDCA_SUBSET_NONE");
}


static void test_feature_list_eq(void) {
   DDCA_Feature_List a = DDCA_EMPTY_FEATURE_LIST;
   DDCA_Feature_List b = DDCA_EMPTY_FEATURE_LIST;
   CK(ddca_feature_list_eq(a, b));

   a = ddca_feature_list_add(&a, 0x60);
   CK(!ddca_feature_list_eq(a, b));

   b = ddca_feature_list_add(&b, 0x60);
   CK(ddca_feature_list_eq(a, b));
}


static void test_feature_list_or_and_and_not(void) {
   DDCA_Feature_List a = DDCA_EMPTY_FEATURE_LIST;
   a = ddca_feature_list_add(&a, 0x10);
   a = ddca_feature_list_add(&a, 0x12);

   DDCA_Feature_List b = DDCA_EMPTY_FEATURE_LIST;
   b = ddca_feature_list_add(&b, 0x12);
   b = ddca_feature_list_add(&b, 0x14);

   DDCA_Feature_List orResult = ddca_feature_list_or(a, b);
   CK(ddca_feature_list_contains(orResult, 0x10));
   CK(ddca_feature_list_contains(orResult, 0x12));
   CK(ddca_feature_list_contains(orResult, 0x14));

   DDCA_Feature_List andResult = ddca_feature_list_and(a, b);
   CK(!ddca_feature_list_contains(andResult, 0x10));
   CK(ddca_feature_list_contains(andResult, 0x12));
   CK(!ddca_feature_list_contains(andResult, 0x14));

   DDCA_Feature_List andNotResult = ddca_feature_list_and_not(a, b);
   CK(ddca_feature_list_contains(andNotResult, 0x10));
   CK(!ddca_feature_list_contains(andNotResult, 0x12));
   CK(!ddca_feature_list_contains(andNotResult, 0x14));
}


static void test_feature_list_count_and_string(void) {
   DDCA_Feature_List list = DDCA_EMPTY_FEATURE_LIST;
   CK_INT(ddca_feature_list_count(list), 0);

   list = ddca_feature_list_add(&list, 0x10);
   list = ddca_feature_list_add(&list, 0x60);
   CK_INT(ddca_feature_list_count(list), 2);

   const char * s = ddca_feature_list_string(list, "0x", ", ");
   CK(s != NULL);
   CK_STR_CONTAINS(s, "0x10");
   CK_STR_CONTAINS(s, "0x60");
}


static void test_get_feature_name(void) {
   CK_STR(ddca_get_feature_name(0x10), "Brightness");
   CK_STR(ddca_get_feature_name(0x00), "unrecognized feature");
   CK_STR(ddca_get_feature_name(0xf5), "manufacturer specific feature");
}


int main(int argc, char ** argv) {
   init_vcp_feature_codes();

   test_feature_list_clear_add_contains();
   test_feature_list_id_name();
   test_feature_list_eq();
   test_feature_list_or_and_and_not();
   test_feature_list_count_and_string();
   test_get_feature_name();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
