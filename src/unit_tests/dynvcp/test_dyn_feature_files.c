/** @file test_dyn_feature_files.c
 *
 *  Standalone unit tests for src/dynvcp/dyn_feature_files.c:
 *  dfr_find_feature_def_file(), dfr_load_by_mmk() (the not-found path;
 *  parsing an actual .mccs file is the responsibility of
 *  create_dynamic_features_rec() in base/dynamic_features.c, not this
 *  file), and dfr_check_by_dref()/dfr_check_by_dh().
 *
 *  main() below points $XDG_DATA_HOME and $XDG_DATA_DIRS at a freshly
 *  created, empty temporary directory before running any test (the same
 *  sandboxing approach used by test_ddc_dumpload.c and
 *  test_dyn_feature_codes.c), so file lookups reliably miss regardless of
 *  the real environment -- no real file I/O is depended on.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: dynvcp source files cross-reference vcp/base/ddc
 *  extensively, so it links the top-level libcommon convenience library
 *  (the same aggregate the ddcutil executable itself links).
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

#include "util/edid.h"
#include "util/error_info.h"

#include "base/displays.h"
#include "base/dynamic_features.h"
#include "base/monitor_model_key.h"

#include "dynvcp/dyn_feature_files.h"

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


static void test_dfr_find_feature_def_file_not_found(void) {
   char * fqfn = dfr_find_feature_def_file("NoSuchModel_ZZZ999");
   CK(fqfn == NULL);
   free(fqfn);
}


static void test_dfr_load_by_mmk_not_found(void) {
   Monitor_Model_Key mmk = mmk_value("ACM", "UnitTestModel", 0x1234);
   Dynamic_Features_Rec * dfr = NULL;

   Error_Info * errs = dfr_load_by_mmk(mmk, &dfr);
   CK(errs != NULL);
   if (errs)
      CK_INT(errs->status_code, DDCRC_NOT_FOUND);

   // a dummy record is still returned, so repeated lookups can be avoided
   CK(dfr != NULL);
   if (dfr) {
      CK(dfr->flags & DFR_FLAGS_NOT_FOUND);
      CK_INT(dfr->product_code, 0x1234);
      dfr_free(dfr);
   }
   if (errs)
      errinfo_free(errs);
}


static Parsed_Edid * make_valid_edid(Byte fill_byte) {
   Byte raw[128];
   memset(raw, fill_byte, 128);
   raw[0] = 0x00; raw[1] = raw[2] = raw[3] = raw[4] = raw[5] = raw[6] = 0xff; raw[7] = 0x00;
   int sum = 0;
   for (int i = 0; i < 127; i++)
      sum += raw[i];
   raw[127] = (Byte)((256 - (sum % 256)) % 256);
   return create_parsed_edid2(raw, "TEST");
}


static void test_dfr_check_by_dref_disabled(void) {
   bool saved = enable_dynamic_features;
   enable_dynamic_features = false;

   Display_Ref * dref = create_bus_display_ref(221);
   dref->pedid = make_valid_edid(0x11);

   Error_Info * errs = dfr_check_by_dref(dref);
   CK(errs == NULL);
   CK(dref->dfr == NULL);
   CK(!(dref->flags & DREF_DYNAMIC_FEATURES_CHECKED));

   enable_dynamic_features = saved;
}


static void test_dfr_check_by_dref_enabled(void) {
   bool saved = enable_dynamic_features;
   enable_dynamic_features = true;

   Display_Ref * dref = create_bus_display_ref(222);
   dref->pedid = make_valid_edid(0x22);

   Error_Info * errs = dfr_check_by_dref(dref);
   // no feature file exists for this fabricated EDID under the sandboxed
   // XDG dirs, so this reports DDCRC_NOT_FOUND, and a dummy dfr is cached
   // on the dref so the check is not repeated.
   CK(errs != NULL);
   if (errs) {
      CK_INT(errs->status_code, DDCRC_NOT_FOUND);
      errinfo_free(errs);
   }
   CK(dref->dfr != NULL);
   CK(dref->flags & DREF_DYNAMIC_FEATURES_CHECKED);

   // a second call must be a no-op (already checked): no error, dfr unchanged
   Dynamic_Features_Rec * dfr_before = dref->dfr;
   Error_Info * errs2 = dfr_check_by_dref(dref);
   CK(errs2 == NULL);
   CK(dref->dfr == dfr_before);

   enable_dynamic_features = saved;
}


static void test_dfr_check_by_dh(void) {
   bool saved = enable_dynamic_features;
   enable_dynamic_features = true;

   Display_Ref * dref = create_bus_display_ref(223);
   dref->pedid = make_valid_edid(0x33);
   Display_Handle * dh = create_base_display_handle(-1, dref);

   Error_Info * errs = dfr_check_by_dh(dh);
   CK(errs != NULL);
   if (errs) {
      CK_INT(errs->status_code, DDCRC_NOT_FOUND);
      errinfo_free(errs);
   }
   CK(dh->dref->dfr != NULL);

   free(dh);
   enable_dynamic_features = saved;
}


int main(int argc, char ** argv) {
   char tmpl[] = "/tmp/ddcutil_test_xdg_data_XXXXXX";
   char * tmpdir = mkdtemp(tmpl);
   if (!tmpdir) {
      fprintf(stderr, "mkdtemp() failed: %s\n", strerror(errno));
      return 2;
   }
   setenv("XDG_DATA_HOME", tmpdir, 1);
   setenv("XDG_DATA_DIRS", tmpdir, 1);

   test_dfr_find_feature_def_file_not_found();
   test_dfr_load_by_mmk_not_found();
   test_dfr_check_by_dref_disabled();
   test_dfr_check_by_dref_enabled();
   test_dfr_check_by_dh();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
