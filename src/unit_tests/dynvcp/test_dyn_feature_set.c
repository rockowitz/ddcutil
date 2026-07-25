/** @file test_dyn_feature_set.c
 *
 *  Standalone unit tests for src/dynvcp/dyn_feature_set.c:
 *  test_show_feature() (the subset-membership predicate),
 *  create_vcp_feature_set()/dyn_create_feature_set() for a representative
 *  sample of subset ids, create_vcp_feature_set_from_feature_set_ref()/
 *  create_dyn_feature_set_from_feature_set_ref() for VCP_SUBSET_MULTI_FEATURES,
 *  and the Dyn_Feature_Set accessors/free function.
 *
 *  dyn_create_feature_set()/dyn_get_feature_metadata_by_dref() are called
 *  here with a NULL Display_Ref throughout, which forces FSF_CHECK_UDF off
 *  and never queries a VCP version from hardware -- see
 *  dyn_get_feature_metadata_by_dref() in dyn_feature_codes.c. So this
 *  never touches user-defined feature files or real displays.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: dynvcp source files cross-reference vcp/base
 *  extensively, so it links the top-level libcommon convenience library
 *  (the same aggregate the ddcutil executable itself links).
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/data_structures.h"

#include "base/feature_set_ref.h"

#include "vcp/vcp_feature_codes.h"

#include "dynvcp/dyn_feature_set.h"
#include "dynvcp/vcp_feature_set.h"

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

/* Runs statement `stmt` with stdout (fd 1) redirected to a temporary file,
 * discarding the captured output. */
#define QUIETLY(stmt) do { \
   fflush(stdout); \
   int _saved = dup(fileno(stdout)); \
   FILE * _tmp = tmpfile(); \
   dup2(fileno(_tmp), fileno(stdout)); \
   stmt; \
   fflush(stdout); \
   dup2(_saved, fileno(stdout)); \
   close(_saved); \
   fclose(_tmp); \
} while(0)


static void test_test_show_feature(void) {
   CK(test_show_feature(VCP_SUBSET_CONT, 0, 0, DDCA_CONT, 0));
   CK(!test_show_feature(VCP_SUBSET_CONT, 0, 0, DDCA_NC, 0));

   // table-exclusion overrides subset membership entirely
   CK(!test_show_feature(VCP_SUBSET_TABLE, FSF_NOTABLE, 0, DDCA_TABLE, 0));
   CK(test_show_feature(VCP_SUBSET_TABLE, 0, 0, DDCA_TABLE, 0));

   // VCP_SUBSET_KNOWN is unconditional (barring table exclusion)
   CK(test_show_feature(VCP_SUBSET_KNOWN, 0, 0, 0, 0));

   // RW/RO/WO-only filters
   CK(!test_show_feature(VCP_SUBSET_KNOWN, FSF_RW_ONLY, 0, DDCA_RO, 0));
   CK(test_show_feature(VCP_SUBSET_KNOWN, FSF_RW_ONLY, 0, DDCA_RW, 0));

   // readable-only filter: DDCA_READABLE is itself the combined mask
   // (DDCA_RO | DDCA_RW), so a plain DDCA_RO flag already satisfies it
   CK(!test_show_feature(VCP_SUBSET_KNOWN, FSF_RO_ONLY, 0, DDCA_WO, 0));   // write-only, not readable
   CK(test_show_feature(VCP_SUBSET_KNOWN, FSF_RO_ONLY, 0, DDCA_RO, 0));
}


static void test_create_vcp_feature_set_known(void) {
   DDCA_MCCS_Version_Spec v22 = {2,2};
   VCP_Feature_Set * fset = create_vcp_feature_set(VCP_SUBSET_KNOWN, v22, 0);
   CK(fset != NULL);
   if (fset)
      CK_INT(get_vcp_feature_set_size(fset), vcp_get_feature_code_count());
   free_vcp_feature_set(fset);
}


static void test_create_vcp_feature_set_table(void) {
   DDCA_MCCS_Version_Spec v22 = {2,2};
   VCP_Feature_Set * fset = create_vcp_feature_set(VCP_SUBSET_TABLE, v22, 0);
   CK(fset != NULL);
   if (fset) {
      CK(get_vcp_feature_set_size(fset) > 0);
      // every member must be some kind of table feature for v2.2 -- DDCA_TABLE
      // is (DDCA_NORMAL_TABLE | DDCA_WO_TABLE), broader than
      // is_table_feature_by_vcp_version(), which tests DDCA_NORMAL_TABLE alone
      for (int i = 0; i < get_vcp_feature_set_size(fset); i++) {
         VCP_Feature_Table_Entry * e = get_vcp_feature_set_entry(fset, i);
         CK(get_version_sensitive_feature_flags(e, v22) & DDCA_TABLE);
      }
   }
   free_vcp_feature_set(fset);
}


static void test_dyn_create_feature_set_known(void) {
   Dyn_Feature_Set * fset = dyn_create_feature_set(VCP_SUBSET_KNOWN, NULL, 0);
   CK(fset != NULL);
   if (fset)
      CK_INT(dyn_get_feature_set_size(fset), vcp_get_feature_code_count());
   dyn_free_feature_set(fset);
}


static void test_dyn_create_feature_set_udf_no_dref(void) {
   // FSF_CHECK_UDF requested, but dref == NULL -> dyn_create_feature_set()
   // clears FSF_CHECK_UDF itself, and the UDF path additionally requires
   // dref->dfr, so this must yield an empty (not NULL) feature set.
   Dyn_Feature_Set * fset = dyn_create_feature_set(VCP_SUBSET_UDF, NULL, FSF_CHECK_UDF);
   CK(fset != NULL);
   if (fset)
      CK_INT(dyn_get_feature_set_size(fset), 0);
   dyn_free_feature_set(fset);
}


static void test_feature_set_ref_multi_features(void) {
   Feature_Set_Ref fsref;
   fsref.subset = VCP_SUBSET_MULTI_FEATURES;
   fsref.features = EMPTY_BIT_SET_256;
   fsref.features = bs256_insert(fsref.features, 0x10);
   fsref.features = bs256_insert(fsref.features, 0x12);
   fsref.features = bs256_insert(fsref.features, 0x60);

   DDCA_MCCS_Version_Spec v22 = {2,2};

   VCP_Feature_Set * vfset = create_vcp_feature_set_from_feature_set_ref(&fsref, v22, 0);
   CK(vfset != NULL);
   if (vfset)
      CK_INT(get_vcp_feature_set_size(vfset), 3);
   free_vcp_feature_set(vfset);

   Dyn_Feature_Set * dfset = create_dyn_feature_set_from_feature_set_ref(&fsref, v22, 0);
   CK(dfset != NULL);
   if (dfset) {
      CK_INT(dyn_get_feature_set_size(dfset), 3);
      CK(dyn_get_feature_set_entry(dfset, 3) == NULL);   // out of range
   }
   dyn_free_feature_set(dfset);
}


static void test_dyn_feature_set_report_smoke(void) {
   Feature_Set_Ref fsref;
   fsref.subset = VCP_SUBSET_MULTI_FEATURES;
   fsref.features = EMPTY_BIT_SET_256;
   fsref.features = bs256_insert(fsref.features, 0x10);

   DDCA_MCCS_Version_Spec v22 = {2,2};
   Dyn_Feature_Set * dfset = create_dyn_feature_set_from_feature_set_ref(&fsref, v22, 0);
   CK(dfset != NULL);

   char * repr = dyn_feature_set_repr_t(dfset);
   CK(repr != NULL);

   QUIETLY( report_dyn_feature_set(dfset, 0) );
   QUIETLY( dbgrpt_dyn_feature_set(dfset, true, 0) );
   CK(true);   // reaching here without crashing is the test

   dyn_free_feature_set(dfset);
}


int main(int argc, char ** argv) {
   init_vcp_feature_codes();

   test_test_show_feature();
   test_create_vcp_feature_set_known();
   test_create_vcp_feature_set_table();
   test_dyn_create_feature_set_known();
   test_dyn_create_feature_set_udf_no_dref();
   test_feature_set_ref_multi_features();
   test_dyn_feature_set_report_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
