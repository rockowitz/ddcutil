/** @file test_app_dynamic_features.c
 *
 *  Standalone unit test for src/app_ddcutil/app_dynamic_features.c:
 *  app_check_dynamic_features() when the global enable_dynamic_features
 *  flag is false, which is a pure early-return with no access to its
 *  Display_Ref argument at all.
 *
 *  When enable_dynamic_features is true, app_check_dynamic_features()
 *  calls dfr_check_by_dref(), which requires a real Display_Ref backed by
 *  a real (or at least fully populated) monitor identity, and so is out
 *  of scope for these unit tests.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libappddcutil/
 *  libcommon convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "dynvcp/dyn_feature_files.h"

#include "app_ddcutil/app_dynamic_features.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)


static void test_disabled_short_circuits(void) {
   bool saved = enable_dynamic_features;
   enable_dynamic_features = false;

   // dref is never dereferenced when the feature is globally disabled
   bool result = app_check_dynamic_features(NULL);
   CK(result == true);

   enable_dynamic_features = saved;
}


int main(int argc, char ** argv) {
   test_disabled_short_circuits();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
