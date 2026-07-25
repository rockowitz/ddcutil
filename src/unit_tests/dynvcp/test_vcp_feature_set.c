/** @file test_vcp_feature_set.c
 *
 *  Standalone unit tests for src/dynvcp/vcp_feature_set.c: the
 *  VCP_Feature_Set accessors (get_vcp_feature_set_entry()/
 *  get_vcp_feature_set_size()), free_vcp_feature_set() (which frees only
 *  synthetic member entries, not ones from the static VCP feature table),
 *  and smoke tests of report_vcp_feature_set()/dbgrpt_vcp_feature_set().
 *  All pure in-memory data structure logic -- no hardware or file I/O.
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

#include "vcp/vcp_feature_codes.h"

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


static VCP_Feature_Set * make_fset(void) {
   VCP_Feature_Set * fset = calloc(1, sizeof(VCP_Feature_Set));
   memcpy(fset->marker, VCP_FEATURE_SET_MARKER, 4);
   fset->subset = VCP_SUBSET_MULTI_FEATURES;
   fset->members = g_ptr_array_new();
   return fset;
}


static void test_get_entry_and_size(void) {
   VCP_Feature_Set * fset = make_fset();
   VCP_Feature_Table_Entry * x10 = vcp_find_feature_by_hexid(0x10);
   VCP_Feature_Table_Entry * x12 = vcp_find_feature_by_hexid(0x12);
   CK(x10 != NULL && x12 != NULL);
   g_ptr_array_add(fset->members, x10);
   g_ptr_array_add(fset->members, x12);

   CK_INT(get_vcp_feature_set_size(fset), 2);
   CK(get_vcp_feature_set_entry(fset, 0) == x10);
   CK(get_vcp_feature_set_entry(fset, 1) == x12);
   CK(get_vcp_feature_set_entry(fset, 2) == NULL);   // out of range

   free_vcp_feature_set(fset);
}


static void test_free_vcp_feature_set_leaves_static_entries_intact(void) {
   VCP_Feature_Set * fset = make_fset();

   // a real, static-table entry
   VCP_Feature_Table_Entry * x10 = vcp_find_feature_by_hexid(0x10);
   g_ptr_array_add(fset->members, x10);

   // a synthetic entry (not in the static table): free_vcp_feature_set()
   // must free this one via free_synthetic_vcp_entry(), since it is
   // otherwise unreachable and would leak.
   VCP_Feature_Table_Entry * synthetic = vcp_create_dummy_feature_for_hexid(0x00);
   CK(synthetic->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY);
   g_ptr_array_add(fset->members, synthetic);

   free_vcp_feature_set(fset);   // must not crash, must not double-free x10

   // x10 must still be intact in the static table afterward
   VCP_Feature_Table_Entry * x10_again = vcp_find_feature_by_hexid(0x10);
   CK(x10_again == x10);
   CK(!(x10_again->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY));
}


static void test_free_vcp_feature_set_null_safe(void) {
   free_vcp_feature_set(NULL);   // must not crash
   CK(true);
}


static void test_report_smoke(void) {
   VCP_Feature_Set * fset = make_fset();
   g_ptr_array_add(fset->members, vcp_find_feature_by_hexid(0x10));
   g_ptr_array_add(fset->members, vcp_find_feature_by_hexid(0x60));

   QUIETLY( report_vcp_feature_set(fset, 0) );
   QUIETLY( dbgrpt_vcp_feature_set(fset, 0) );
   CK(true);   // reaching here without crashing is the test

   free_vcp_feature_set(fset);
}


int main(int argc, char ** argv) {
   init_vcp_feature_codes();

   test_get_entry_and_size();
   test_free_vcp_feature_set_leaves_static_entries_intact();
   test_free_vcp_feature_set_null_safe();
   test_report_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
