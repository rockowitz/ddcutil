/** @file test_vcp_feature_codes.c
 *
 *  Standalone unit tests for src/vcp/vcp_feature_codes.c: the VCP feature
 *  code table lookup functions (vcp_find_feature_by_hexid() and its
 *  _w_default() variant, vcp_get_feature_table_entry()), the
 *  version-sensitive flags/name/sl-values accessors, a representative
 *  sample of the per-feature value-formatting functions, and
 *  extract_version_feature_info_from_feature_table_entry().  All pure
 *  table lookup and string formatting -- no hardware or file I/O.
 *
 *  init_vcp_feature_codes() is called once at startup: its header comment
 *  documents it as required before any other function in the file, and it
 *  is what stamps the VCP_FEATURE_TABLE_ENTRY_MARKER into each static
 *  table entry that dbgrpt_vcp_entry()/free_synthetic_vcp_entry() assert on.
 *
 *  This file does not attempt to exercise all ~150 VCP feature codes or
 *  all of the ~30 per-feature formatting functions; it covers a
 *  representative sample (0x10 Brightness: simple continuous, single
 *  version; 0x60 Input Source: SIMPLE_NC in v2.0/v2.2, Table in v3.0,
 *  version-varying feature name/flags).
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libvcp unit test: it links the internal libvcp/libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/feature_metadata.h"

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


static void test_feature_code_count(void) {
   int ct = vcp_get_feature_code_count();
   CK(ct > 100);   // ~144 entries at time of writing
}


static void test_vcp_get_feature_table_entry(void) {
   VCP_Feature_Table_Entry * e = vcp_get_feature_table_entry(0);
   CK(e != NULL);
   // round trip: looking up entry 0's own code must return the same entry
   if (e) {
      VCP_Feature_Table_Entry * found = vcp_find_feature_by_hexid(e->code);
      CK(found == e);
   }
}


static void test_find_feature_by_hexid(void) {
   VCP_Feature_Table_Entry * x10 = vcp_find_feature_by_hexid(0x10);
   CK(x10 != NULL);
   if (x10) {
      CK_INT(x10->code, 0x10);
      CK_STR(x10->v20_name, "Brightness");
      CK_STR(x10->v30_name, "Luminosity");
      CK(x10->v20_flags & DDCA_RW);
      CK(x10->v20_flags & DDCA_STD_CONT);
   }

   // 0x00 is not a valid/defined VCP feature code
   CK(vcp_find_feature_by_hexid(0x00) == NULL);
}


static void test_find_feature_by_hexid_w_default(void) {
   // a real entry: not synthetic, returned as-is
   VCP_Feature_Table_Entry * x10 = vcp_find_feature_by_hexid_w_default(0x10);
   CK(x10 != NULL);
   if (x10) {
      CK(!(x10->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY));
      // free_synthetic_vcp_entry() must be a no-op for a real (static) table
      // entry -- it must NOT free memory belonging to the static table.
      free_synthetic_vcp_entry(x10);
      VCP_Feature_Table_Entry * x10_again = vcp_find_feature_by_hexid(0x10);
      CK(x10_again == x10);
      CK_STR(x10_again->v20_name, "Brightness");
   }

   // an undefined code: synthesized dummy entry, caller must free
   VCP_Feature_Table_Entry * dummy = vcp_find_feature_by_hexid_w_default(0x00);
   CK(dummy != NULL);
   if (dummy) {
      CK_INT(dummy->code, 0x00);
      CK(dummy->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY);
      CK_STR(dummy->v20_name, "Unknown feature");
      free_synthetic_vcp_entry(dummy);
   }

   // an undefined manufacturer-specific code
   VCP_Feature_Table_Entry * mfg = vcp_find_feature_by_hexid_w_default(0xf5);
   CK(mfg != NULL);
   if (mfg) {
      CK_STR(mfg->v20_name, "Manufacturer Specific");
      free_synthetic_vcp_entry(mfg);
   }
}


static void test_vcp_create_dummy_feature_for_hexid(void) {
   VCP_Feature_Table_Entry * e1 = vcp_create_dummy_feature_for_hexid(0x00);
   CK(e1 != NULL);
   if (e1) {
      CK_STR(e1->v20_name, "Unknown feature");
      CK(e1->vcp_global_flags & DDCA_SYNTHETIC);
      free_synthetic_vcp_entry(e1);
   }

   VCP_Feature_Table_Entry * e2 = vcp_create_dummy_feature_for_hexid(0xe5);
   CK(e2 != NULL);
   if (e2) {
      CK_STR(e2->v20_name, "Manufacturer Specific");
      free_synthetic_vcp_entry(e2);
   }
}


static void test_vcp_create_table_dummy_feature_for_hexid(void) {
   VCP_Feature_Table_Entry * e = vcp_create_table_dummy_feature_for_hexid(0x00);
   CK(e != NULL);
   if (e) {
      CK_STR(e->v20_name, "Unknown feature");
      CK(e->v20_flags & DDCA_NORMAL_TABLE);
      free_synthetic_vcp_entry(e);
   }
}


static void test_readable_writable_table_by_version(void) {
   VCP_Feature_Table_Entry * x10 = vcp_find_feature_by_hexid(0x10);   // RW | STD_CONT, v20 only
   CK(x10 != NULL);
   if (x10) {
      DDCA_MCCS_Version_Spec v20 = {2,0};
      CK(is_feature_readable_by_vcp_version(x10, v20));
      CK(is_feature_writable_by_vcp_version(x10, v20));
      CK(!is_table_feature_by_vcp_version(x10, v20));
      CK(is_feature_supported_in_version(x10, v20));

      // no v30_flags set for x10 -- version-sensitive lookup must fall back
      // to v20_flags rather than reporting the feature unsupported
      DDCA_MCCS_Version_Spec v30 = {3,0};
      CK(is_feature_readable_by_vcp_version(x10, v30));
   }
}


static void test_version_specific_feature_flags_fallback(void) {
   VCP_Feature_Table_Entry * x10 = vcp_find_feature_by_hexid(0x10);
   CK(x10 != NULL);
   if (x10) {
      DDCA_MCCS_Version_Spec v20 = {2,0};
      DDCA_MCCS_Version_Spec v30 = {3,0};
      CK_INT(get_version_specific_feature_flags(x10, v20), x10->v20_flags);
      // x10 has no v21/v22/v30 flags set, so even a v3.0 monitor's flags
      // must fall back to v20_flags
      CK_INT(get_version_specific_feature_flags(x10, v30), x10->v20_flags);
   }
}


static void test_version_specific_feature_name(void) {
   VCP_Feature_Table_Entry * x10 = vcp_find_feature_by_hexid(0x10);
   CK(x10 != NULL);
   if (x10) {
      DDCA_MCCS_Version_Spec v20 = {2,0};
      DDCA_MCCS_Version_Spec v21 = {2,1};
      DDCA_MCCS_Version_Spec v30 = {3,0};

      CK_STR(get_version_specific_feature_name(x10, v20), "Brightness");
      // no v21_name set -> falls back to v20_name
      CK_STR(get_version_specific_feature_name(x10, v21), "Brightness");
      // v30_name is explicitly set to "Luminosity"
      CK_STR(get_version_specific_feature_name(x10, v30), "Luminosity");

      CK_STR(get_non_version_specific_feature_name(x10), "Brightness");
   }
}


static void test_get_feature_name_by_id(void) {
   CK_STR(get_feature_name_by_id_only(0x10), "Brightness");
   CK_STR(get_feature_name_by_id_only(0xf5), "manufacturer specific feature");
   CK_STR(get_feature_name_by_id_only(0x00), "unrecognized feature");

   DDCA_MCCS_Version_Spec v30 = {3,0};
   CK_STR(get_feature_name_by_id_and_vcp_version(0x10, v30), "Luminosity");
}


static void test_has_version_specific_features(void) {
   VCP_Feature_Table_Entry * x10 = vcp_find_feature_by_hexid(0x10);   // only v20_flags set
   VCP_Feature_Table_Entry * x60 = vcp_find_feature_by_hexid(0x60);   // v20/v30/v22 flags set
   CK(x10 != NULL && x60 != NULL);
   if (x10 && x60) {
      CK(!has_version_specific_features(x10));
      CK(has_version_specific_features(x60));
   }
}


static void test_is_version_conditional_vcp_type(void) {
   VCP_Feature_Table_Entry * x10 = vcp_find_feature_by_hexid(0x10);   // always non-table (STD_CONT)
   VCP_Feature_Table_Entry * x60 = vcp_find_feature_by_hexid(0x60);   // NC in v2.x, Table in v3.0
   CK(x10 != NULL && x60 != NULL);
   if (x10 && x60) {
      CK(!is_version_conditional_vcp_type(x10));
      CK(is_version_conditional_vcp_type(x60));
   }
}


static void test_get_highest_non_deprecated_version(void) {
   VCP_Feature_Table_Entry * x10 = vcp_find_feature_by_hexid(0x10);   // only v20_flags
   VCP_Feature_Table_Entry * x60 = vcp_find_feature_by_hexid(0x60);   // v22_flags set, not deprecated
   CK(x10 != NULL && x60 != NULL);
   if (x10) {
      DDCA_MCCS_Version_Spec v = get_highest_non_deprecated_version(x10);
      CK_INT(v.major, 2);
      CK_INT(v.minor, 0);
   }
   if (x60) {
      DDCA_MCCS_Version_Spec v = get_highest_non_deprecated_version(x60);
      CK_INT(v.major, 2);
      CK_INT(v.minor, 2);
   }
}


static void test_get_version_specific_sl_values(void) {
   VCP_Feature_Table_Entry * x60 = vcp_find_feature_by_hexid(0x60);
   CK(x60 != NULL);
   if (x60) {
      CK(x60->default_sl_values != NULL);
      DDCA_MCCS_Version_Spec v20 = {2,0};
      DDCA_MCCS_Version_Spec v22 = {2,2};
      // neither v20 nor v22 sl-value tables are set for x60, so both
      // versions must fall back to default_sl_values
      CK(get_version_specific_sl_values(x60, v20) == x60->default_sl_values);
      CK(get_version_specific_sl_values(x60, v22) == x60->default_sl_values);
   }
}


static void test_sl_lookup_formatting(void) {
   Nontable_Vcp_Value code_info = {0};
   code_info.vcp_code = 0x60;
   code_info.sl = 0x01;   // "VGA-1" per x60_v2_input_source_values
   DDCA_MCCS_Version_Spec v20 = {2,0};

   char buf[100];
   bool ok = format_feature_detail_sl_lookup(&code_info, v20, buf, sizeof(buf));
   CK(ok);
   CK_STR_CONTAINS(buf, "VGA-1");

   // an sl value not present in the table
   code_info.sl = 0xfe;
   ok = format_feature_detail_sl_lookup(&code_info, v20, buf, sizeof(buf));
   CK(ok);
   CK_STR_CONTAINS(buf, "Invalid value");
}


static void test_format_feature_detail_standard_continuous(void) {
   Nontable_Vcp_Value code_info = {0};
   code_info.cur_value = 42;
   code_info.max_value = 99;
   DDCA_MCCS_Version_Spec v20 = {2,0};

   char buf[100];
   bool ok = format_feature_detail_standard_continuous(&code_info, v20, buf, sizeof(buf));
   CK(ok);
   CK_STR_CONTAINS(buf, "42");
   CK_STR_CONTAINS(buf, "99");
}


static void test_format_feature_detail_sl_byte(void) {
   Nontable_Vcp_Value code_info = {0};
   code_info.sl = 0x07;
   DDCA_MCCS_Version_Spec v20 = {2,0};

   char buf[100];
   bool ok = format_feature_detail_sl_byte(&code_info, v20, buf, sizeof(buf));
   CK(ok);
   CK_STR_CONTAINS(buf, "0x07");
}


static void test_format_feature_detail_debug_bytes(void) {
   Nontable_Vcp_Value code_info = {0};
   code_info.mh = 0x01;
   code_info.ml = 0x02;
   code_info.sh = 0x03;
   code_info.sl = 0x04;
   DDCA_MCCS_Version_Spec v20 = {2,0};

   char buf[100];
   bool ok = format_feature_detail_debug_bytes(&code_info, v20, buf, sizeof(buf));
   CK(ok);
   CK_STR_CONTAINS(buf, "mh=0x01");
   CK_STR_CONTAINS(buf, "ml=0x02");
   CK_STR_CONTAINS(buf, "sh=0x03");
   CK_STR_CONTAINS(buf, "sl=0x04");
}


static void test_vcp_interpret_global_feature_flags(void) {
   char buf[100];
   char * r = vcp_interpret_global_feature_flags(DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY, buf, sizeof(buf));
   CK(r == buf);
   CK_STR_CONTAINS(buf, "Synthetic VCP Feature Table Entry");

   vcp_interpret_global_feature_flags(0, buf, sizeof(buf));
   CK_STR(buf, "");
}


static void test_spec_group_names_r(void) {
   VCP_Feature_Table_Entry * x3e = vcp_find_feature_by_hexid(0x3e);   // IMAGE | MISC
   CK(x3e != NULL);
   if (x3e) {
      char buf[100];
      char * r = spec_group_names_r(x3e, buf, sizeof(buf));
      CK(r == buf);
      CK_STR_CONTAINS(buf, "Image");
      CK_STR_CONTAINS(buf, "Miscellaneous");
   }
}


static void test_extract_version_feature_info(void) {
   VCP_Feature_Table_Entry * x10 = vcp_find_feature_by_hexid(0x10);
   CK(x10 != NULL);
   if (x10) {
      DDCA_MCCS_Version_Spec v20 = {2,0};
      Display_Feature_Metadata * dfm =
            extract_version_feature_info_from_feature_table_entry(x10, v20, /*version_sensitive=*/true);
      CK(dfm != NULL);
      if (dfm) {
         CK_INT(dfm->feature_code, 0x10);
         CK_STR(dfm->feature_name, "Brightness");
         CK(dfm->version_feature_flags & DDCA_RW);
         CK(dfm->sl_values == NULL);   // x10 has no sl value table
         dfm_free(dfm);
      }
   }

   VCP_Feature_Table_Entry * x60 = vcp_find_feature_by_hexid(0x60);
   CK(x60 != NULL);
   if (x60) {
      DDCA_MCCS_Version_Spec v20 = {2,0};
      Display_Feature_Metadata * dfm =
            extract_version_feature_info_from_feature_table_entry(x60, v20, /*version_sensitive=*/true);
      CK(dfm != NULL);
      if (dfm) {
         CK_STR(dfm->feature_name, "Input Source");
         // sl_values must be a deep copy, not the same pointer as the table
         CK(dfm->sl_values != NULL);
         CK(dfm->sl_values != x60->default_sl_values);
         dfm_free(dfm);
      }
   }
}


static void test_find_feature_values_for_capabilities(void) {
   DDCA_MCCS_Version_Spec v20 = {2,0};
   DDCA_Feature_Value_Entry * vals = find_feature_values_for_capabilities(0x60, v20);
   CK(vals != NULL);

   // manufacturer-specific / unrecognized code -> NULL, must not crash
   DDCA_Feature_Value_Entry * none = find_feature_values_for_capabilities(0xf5, v20);
   CK(none == NULL);
}


int main(int argc, char ** argv) {
   init_vcp_feature_codes();

   test_feature_code_count();
   test_vcp_get_feature_table_entry();
   test_find_feature_by_hexid();
   test_find_feature_by_hexid_w_default();
   test_vcp_create_dummy_feature_for_hexid();
   test_vcp_create_table_dummy_feature_for_hexid();
   test_readable_writable_table_by_version();
   test_version_specific_feature_flags_fallback();
   test_version_specific_feature_name();
   test_get_feature_name_by_id();
   test_has_version_specific_features();
   test_is_version_conditional_vcp_type();
   test_get_highest_non_deprecated_version();
   test_get_version_specific_sl_values();
   test_sl_lookup_formatting();
   test_format_feature_detail_standard_continuous();
   test_format_feature_detail_sl_byte();
   test_format_feature_detail_debug_bytes();
   test_vcp_interpret_global_feature_flags();
   test_spec_group_names_r();
   test_extract_version_feature_info();
   test_find_feature_values_for_capabilities();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
