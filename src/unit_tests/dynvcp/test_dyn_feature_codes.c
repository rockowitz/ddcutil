/** @file test_dyn_feature_codes.c
 *
 *  Standalone unit tests for src/dynvcp/dyn_feature_codes.c: the sl-value
 *  lookup formatters, dyn_get_feature_metadata_by_dfr_and_vspec_dfm() (the
 *  common core, called with dfr == NULL to exercise its fallback to the
 *  static VCP feature table), dyn_get_feature_metadata_by_dref()/
 *  dyn_get_feature_metadata_by_dh() (which only ever read an *existing*
 *  dref->dfr -- they never call dfr_load_by_mmk() themselves, so no file
 *  I/O is possible from these two regardless of check_udf), the format
 *  dispatch functions, and dyn_get_feature_name().
 *
 *  dyn_get_feature_metadata_by_mmk_and_vspec() with check_udf=true does
 *  call dfr_load_by_mmk(), so that one test sandboxes
 *  $XDG_DATA_HOME/$XDG_DATA_DIRS the same way test_ddc_dumpload.c and
 *  test_dyn_feature_files.c do.
 *
 *  Every Display_Ref/Display_Handle used here has vcp_version_xdf
 *  pre-set, so get_vcp_version_by_dref()/get_vcp_version_by_dh() never
 *  reach their real-DDC-communication fallback path.
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

#include "base/displays.h"
#include "base/feature_metadata.h"
#include "base/monitor_model_key.h"

#include "vcp/vcp_feature_codes.h"
#include "vcp/vcp_feature_values.h"

#include "dynvcp/dyn_feature_codes.h"

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


static void test_sl_lookup_formatters(void) {
   DDCA_Feature_Value_Entry table[] = {
      {0x01, "Foo"},
      {0x02, "Bar"},
      {0x00, NULL},
   };
   Nontable_Vcp_Value code_info = {0};
   code_info.sl = 0x01;
   code_info.sh = 0x07;
   char buf[100];

   CK(dyn_format_feature_detail_sl_lookup(&code_info, table, buf, sizeof(buf)));
   CK_STR_CONTAINS(buf, "Foo");

   code_info.sl = 0x03;   // not in table
   CK(dyn_format_feature_detail_sl_lookup(&code_info, table, buf, sizeof(buf)));
   CK_STR_CONTAINS(buf, "Unrecognized value");

   code_info.sl = 0x01;
   CK(dyn_format_feature_detail_sl_lookup(&code_info, NULL, buf, sizeof(buf)));
   CK_STR(buf, "0x01");

   CK(dyn_format_feature_detail_sl_lookup_with_sh(&code_info, table, buf, sizeof(buf)));
   CK_STR_CONTAINS(buf, "Foo");
   CK_STR_CONTAINS(buf, "0x07");   // sh
}


static void test_dyn_get_feature_metadata_by_dfr_and_vspec_dfm(void) {
   DDCA_MCCS_Version_Spec v20 = {2,0};

   // dfr == NULL -> falls back to the static VCP feature table
   Display_Feature_Metadata * dfm =
         dyn_get_feature_metadata_by_dfr_and_vspec_dfm(0x10, NULL, v20, /*with_default=*/false);
   CK(dfm != NULL);
   if (dfm) {
      CK_STR(dfm->feature_name, "Brightness");
      CK(dfm->nontable_formatter != NULL);   // DDCA_STD_CONT
      dfm_free(dfm);
   }

   // unrecognized code, with_default=false -> NULL
   Display_Feature_Metadata * none =
         dyn_get_feature_metadata_by_dfr_and_vspec_dfm(0x00, NULL, v20, false);
   CK(none == NULL);

   // unrecognized code, with_default=true -> synthesized dummy entry
   Display_Feature_Metadata * dummy =
         dyn_get_feature_metadata_by_dfr_and_vspec_dfm(0x00, NULL, v20, true);
   CK(dummy != NULL);
   if (dummy)
      dfm_free(dummy);
}


static void test_dyn_get_feature_metadata_by_mmk_and_vspec(void) {
   char tmpl[] = "/tmp/ddcutil_test_xdg_data_XXXXXX";
   char * tmpdir = mkdtemp(tmpl);
   if (!tmpdir) {
      fprintf(stderr, "mkdtemp() failed: %s\n", strerror(errno));
      exit(2);
   }
   setenv("XDG_DATA_HOME", tmpdir, 1);
   setenv("XDG_DATA_DIRS", tmpdir, 1);

   Monitor_Model_Key mmk = mmk_value("ACM", "UnitTestModel", 0x1234);
   DDCA_MCCS_Version_Spec v20 = {2,0};

   // check_udf=false: no file lookup at all
   Display_Feature_Metadata * dfm1 =
         dyn_get_feature_metadata_by_mmk_and_vspec(0x10, mmk, v20, /*check_udf=*/false, false);
   CK(dfm1 != NULL);
   if (dfm1) {
      CK_STR(dfm1->feature_name, "Brightness");
      dfm_free(dfm1);
   }

   // check_udf=true: looks for a feature file for this (fabricated, surely
   // nonexistent) model under the sandboxed XDG dirs, finds nothing, and
   // falls back to the static table just the same.
   Display_Feature_Metadata * dfm2 =
         dyn_get_feature_metadata_by_mmk_and_vspec(0x10, mmk, v20, /*check_udf=*/true, false);
   CK(dfm2 != NULL);
   if (dfm2) {
      CK_STR(dfm2->feature_name, "Brightness");
      dfm_free(dfm2);
   }
}


static void test_dyn_get_feature_metadata_by_dref(void) {
   // dref == NULL: pure, no hardware access
   Display_Feature_Metadata * dfm_no_dref =
         dyn_get_feature_metadata_by_dref(0x10, NULL, /*check_udf=*/true, false);
   CK(dfm_no_dref != NULL);
   if (dfm_no_dref)
      dfm_free(dfm_no_dref);

   // dref set, with vcp_version_xdf pre-set so get_vcp_version_by_dref()
   // never reaches its real-DDC-communication fallback; dref->dfr is NULL,
   // and dyn_get_feature_metadata_by_dref() never calls dfr_load_by_mmk()
   // itself, so this is also pure.
   Display_Ref * dref = create_bus_display_ref(211);
   dref->vcp_version_xdf.major = 2;
   dref->vcp_version_xdf.minor = 1;

   Display_Feature_Metadata * dfm =
         dyn_get_feature_metadata_by_dref(0x10, dref, /*check_udf=*/true, false);
   CK(dfm != NULL);
   if (dfm) {
      CK(dfm->display_ref == dref);
      dfm_free(dfm);
   }
}


static void test_dyn_get_feature_metadata_by_dh(void) {
   Display_Ref * dref = create_bus_display_ref(212);
   dref->vcp_version_xdf.major = 2;
   dref->vcp_version_xdf.minor = 2;
   Display_Handle * dh = create_base_display_handle(-1, dref);

   Display_Feature_Metadata * dfm =
         dyn_get_feature_metadata_by_dh(0x10, dh, /*check_udf=*/true, false);
   CK(dfm != NULL);
   if (dfm) {
      CK(dfm->display_ref == dref);
      dfm_free(dfm);
   }

   free(dh);
}


static void test_dyn_format_feature_detail(void) {
   DDCA_MCCS_Version_Spec v20 = {2,0};
   Display_Feature_Metadata * dfm =
         dyn_get_feature_metadata_by_dfr_and_vspec_dfm(0x10, NULL, v20, false);
   CK(dfm != NULL);
   if (!dfm)
      return;

   Nontable_Vcp_Value code_info = {0};
   code_info.cur_value = 42;
   code_info.max_value = 99;
   char buf[100];
   CK(dyn_format_nontable_feature_detail(dfm, &code_info, buf, sizeof(buf)));
   CK_STR_CONTAINS(buf, "42");
   CK_STR_CONTAINS(buf, "99");

   DDCA_Any_Vcp_Value * valrec = create_cont_vcp_value(0x10, 99, 42);
   char * formatted = NULL;
   CK(dyn_format_feature_detail(dfm, v20, valrec, &formatted));
   CK(formatted != NULL);
   if (formatted) {
      CK_STR_CONTAINS(formatted, "42");
      free(formatted);
   }
   free_single_vcp_value(valrec);

   dfm_free(dfm);
}


static void test_dyn_get_feature_name(void) {
   CK_STR(dyn_get_feature_name(0x10, NULL), "Brightness");

   Display_Ref * dref = create_bus_display_ref(213);
   dref->vcp_version_xdf.major = 3;
   dref->vcp_version_xdf.minor = 0;
   // dref->dfr is NULL, so this falls back to get_feature_name_by_id_and_vcp_version()
   CK_STR(dyn_get_feature_name(0x10, dref), "Luminosity");   // v30_name for x10
}


int main(int argc, char ** argv) {
   init_vcp_feature_codes();

   test_sl_lookup_formatters();
   test_dyn_get_feature_metadata_by_dfr_and_vspec_dfm();
   test_dyn_get_feature_metadata_by_mmk_and_vspec();
   test_dyn_get_feature_metadata_by_dref();
   test_dyn_get_feature_metadata_by_dh();
   test_dyn_format_feature_detail();
   test_dyn_get_feature_name();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
