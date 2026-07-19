/** @file test_feature_set_ref.c
 *
 *  Standalone unit tests for the name/repr functions in src/base/feature_set_ref.c:
 *  the VCP feature-subset name (symbolic) and names (title list), and the
 *  feature-set flag names.
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

#include "base/feature_set_ref.h"

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

#define CK_HAS(actual, needle) do { \
   total++; \
   const char * _a = (actual); const char * _n = (needle); \
   if (_a == NULL || strstr(_a, _n) == NULL) { failed++; \
      printf("FAIL  line %-4d  \"%s\" does not contain \"%s\"\n", __LINE__, \
             _a ? _a : "(null)", _n); } \
} while(0)

int main(int argc, char ** argv) {
   // feature_subset_name returns the symbolic (stringified) constant
   CK_STR(feature_subset_name(VCP_SUBSET_COLOR),   "VCP_SUBSET_COLOR");
   CK_STR(feature_subset_name(VCP_SUBSET_PROFILE), "VCP_SUBSET_PROFILE");
   CK_STR(feature_subset_name(VCP_SUBSET_AUDIO),   "VCP_SUBSET_AUDIO");

   // feature_subset_names joins the short titles of the set subset flags
   char * s1 = feature_subset_names(VCP_SUBSET_COLOR);
   CK_STR(s1, "COLOR");
   free(s1);

   char * s2 = feature_subset_names(VCP_SUBSET_COLOR | VCP_SUBSET_AUDIO);
   CK_STR(s2, "COLOR, AUDIO");     // in table order
   free(s2);

   char * s3 = feature_subset_names(0);
   CK_STR(s3, "");                 // nothing set
   free(s3);

   // feature_set_flag_names_t names the set flags, joined by '|'
   CK_STR(feature_set_flag_names_t(FSF_NOTABLE), "FSF_NOTABLE");
   CK_HAS(feature_set_flag_names_t(FSF_SHOW_UNSUPPORTED | FSF_NOTABLE), "FSF_SHOW_UNSUPPORTED");
   CK_HAS(feature_set_flag_names_t(FSF_SHOW_UNSUPPORTED | FSF_NOTABLE), "FSF_NOTABLE");

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
