/** @file test_dynamic_features.c
 *
 *  Standalone unit tests for src/base/dynamic_features.c: creation of a
 *  Dynamic_Features_Rec (dfr_new), its string repr (dfr_repr_t), the flag
 *  interpreter, and a feature-metadata lookup miss on a record with no features.
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
#include "base/dynamic_features.h"

// Defined and exported by dynamic_features.c, not declared in its header.
extern const char * interpret_dfr_flags_symbolic_t(DFR_Flags flags);

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

static void test_dfr_new(void) {
   Dynamic_Features_Rec * dfr = dfr_new("DEL", "Model1", 0x1234, "/tmp/dfr");
   CK(dfr != NULL);
   CK(memcmp(dfr->marker, "DFRC", 4) == 0);
   CK_STR(dfr->mfg_id, "DEL");
   CK_STR(dfr->model_name, "Model1");
   CK_INT(dfr->product_code, 0x1234);
   CK_STR(dfr->filename, "/tmp/dfr");
   CK(dfr->features == NULL);      // no features loaded

   // repr: "Dynamic_Features_Rec[mfg,model,product]"
   CK_STR(dfr_repr_t(dfr), "Dynamic_Features_Rec[DEL,Model1,4660]");

   // lookup on a record with no features returns NULL
   CK(dyn_get_dynamic_feature_metadata(dfr, 0x10) == NULL);

   dfr_free(dfr);

   // NULL filename is allowed
   Dynamic_Features_Rec * dfr2 = dfr_new("SAM", "M2", 1, NULL);
   CK(dfr2->filename == NULL);
   dfr_free(dfr2);

   // repr of NULL
   CK_STR(dfr_repr_t(NULL), "NULL");
}

static void test_flag_interpreter(void) {
   CK_STR(interpret_dfr_flags_symbolic_t(DFR_FLAGS_NONE), "DFR_FLAGS_NONE");
   CK_STR(interpret_dfr_flags_symbolic_t(DFR_FLAGS_NOT_FOUND), "DFR_FLAGS_NOT_FOUND");
   CK_STR(interpret_dfr_flags_symbolic_t(DFR_FLAGS_NOT_FOUND | DFR_FLAG_EXCLUDE_FROM_API),
          "DFR_FLAGS_NOT_FOUND|DFR_FLAG_EXCLUDE_FROM_API");
}

int main(int argc, char ** argv) {
   test_dfr_new();
   test_flag_interpreter();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
