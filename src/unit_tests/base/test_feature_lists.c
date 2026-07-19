/** @file test_feature_lists.c
 *
 *  Standalone unit tests for src/base/feature_lists.c: the DDCA_Feature_List
 *  bit-set operations (clear/add/contains/count) and set algebra (or/and/and_not)
 *  plus the string representation.
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
#include "base/feature_lists.h"

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

static DDCA_Feature_List of2(uint8_t a, uint8_t b) {
   DDCA_Feature_List fl;
   feature_list_clear(&fl);
   feature_list_add(&fl, a);
   feature_list_add(&fl, b);
   return fl;
}

static void test_basic(void) {
   DDCA_Feature_List fl;
   feature_list_clear(&fl);
   CK_INT(feature_list_count(&fl), 0);
   CK(feature_list_contains(&fl, 0x10) == false);

   feature_list_add(&fl, 0x10);
   CK(feature_list_contains(&fl, 0x10) == true);
   CK(feature_list_contains(&fl, 0x11) == false);
   CK_INT(feature_list_count(&fl), 1);

   feature_list_add(&fl, 0x20);
   feature_list_add(&fl, 0xff);    // boundary value
   CK_INT(feature_list_count(&fl), 3);
   CK(feature_list_contains(&fl, 0xff) == true);

   // adding a value twice does not double-count
   feature_list_add(&fl, 0x10);
   CK_INT(feature_list_count(&fl), 3);
}

static void test_set_algebra(void) {
   DDCA_Feature_List a = of2(0x10, 0x20);
   DDCA_Feature_List b = of2(0x20, 0x30);

   DDCA_Feature_List u = feature_list_or(&a, &b);      // {0x10,0x20,0x30}
   CK_INT(feature_list_count(&u), 3);
   CK(feature_list_contains(&u, 0x10) && feature_list_contains(&u, 0x30));

   DDCA_Feature_List i = feature_list_and(&a, &b);     // {0x20}
   CK_INT(feature_list_count(&i), 1);
   CK(feature_list_contains(&i, 0x20) == true);
   CK(feature_list_contains(&i, 0x10) == false);

   DDCA_Feature_List d = feature_list_and_not(&a, &b); // a minus b = {0x10}
   CK_INT(feature_list_count(&d), 1);
   CK(feature_list_contains(&d, 0x10) == true);
   CK(feature_list_contains(&d, 0x20) == false);
}

static void test_string(void) {
   DDCA_Feature_List fl = of2(0x10, 0x20);
   const char * s = feature_list_string(&fl, "x", ",");
   CK(strstr(s, "x10") != NULL);
   CK(strstr(s, "x20") != NULL);
}

int main(int argc, char ** argv) {
   test_basic();
   test_set_algebra();
   test_string();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
