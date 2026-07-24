/** @file test_ddc_display_selection.c
 *
 *  Standalone unit tests for src/ddc/ddc_display_selection.c:
 *  ddc_find_display_ref_by_selector(), which searches the global
 *  #all_display_refs list (guarded by #all_display_refs_mutex, both
 *  extern in base/displays.h) for the first non-phantom Display_Ref
 *  matching a #Display_Selector's criteria.
 *
 *  Rather than triggering real display detection, this test populates
 *  #all_display_refs directly with fabricated Display_Ref instances (the
 *  same "populate the detection-result global directly" pattern used for
 *  #all_i2c_buses and #sys_drm_connectors in the i2c/sysfs unit tests) --
 *  no hardware or file I/O is involved.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: ddc source files cross-reference each other
 *  and the rest of the ddcutil core extensively, so it links the full
 *  top-level libcommon convenience library (the same aggregate the
 *  ddcutil executable itself links) rather than a minimal per-directory
 *  library set.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/coredefs.h"
#include "util/edid.h"

#include "base/displays.h"

#include "ddc/ddc_display_selection.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)


static Parsed_Edid * make_edid(const char * mfg_id, const char * model_name,
                                const char * serial_ascii, Byte fill_byte)
{
   Parsed_Edid * edid = calloc(1, sizeof(Parsed_Edid));
   memcpy(edid->marker, EDID_MARKER_NAME, 4);
   memset(edid->bytes, fill_byte, 128);
   STRLCPY(edid->mfg_id, mfg_id, sizeof(edid->mfg_id));
   STRLCPY(edid->model_name, model_name, sizeof(edid->model_name));
   STRLCPY(edid->serial_ascii, serial_ascii, sizeof(edid->serial_ascii));
   return edid;
}


static Display_Ref * make_dref(int busno, int dispno, Parsed_Edid * edid) {
   Display_Ref * dref = create_bus_display_ref(busno);
   dref->dispno = dispno;
   dref->pedid = edid;
   return dref;
}


static Display_Ref * dref1;   // busno 101, dispno 1, ACM/AlphaModel/SN001
static Display_Ref * dref2;   // busno 102, dispno 2, ACM/BetaModel/SN002
static Display_Ref * dref_phantom;   // busno 103, DISPNO_PHANTOM, same edid as dref1


static void setup_all_display_refs(void) {
   dref1 = make_dref(101, 1, make_edid("ACM", "AlphaModel", "SN001", 0x01));
   dref2 = make_dref(102, 2, make_edid("ACM", "BetaModel",  "SN002", 0x02));
   dref_phantom = make_dref(103, DISPNO_PHANTOM, make_edid("ACM", "AlphaModel", "SN001", 0x01));

   all_display_refs = g_ptr_array_new();
   g_ptr_array_add(all_display_refs, dref1);
   g_ptr_array_add(all_display_refs, dref2);
   g_ptr_array_add(all_display_refs, dref_phantom);
}


static void test_find_by_busno(void) {
   Display_Selector * dsel = dsel_new();
   dsel->busno = 102;
   CK(ddc_find_display_ref_by_selector(dsel) == dref2);
   dsel_free(dsel);
}


static void test_find_by_dispno(void) {
   Display_Selector * dsel = dsel_new();
   dsel->dispno = 1;
   CK(ddc_find_display_ref_by_selector(dsel) == dref1);
   dsel_free(dsel);
}


static void test_find_by_mfg_model_serial(void) {
   Display_Selector * dsel = dsel_new();
   dsel->mfg_id = strdup("acm");         // case-insensitive match
   dsel->model_name = strdup("BetaModel");
   dsel->serial_ascii = strdup("SN002");
   CK(ddc_find_display_ref_by_selector(dsel) == dref2);
   dsel_free(dsel);
}


static void test_no_match(void) {
   Display_Selector * dsel = dsel_new();
   dsel->busno = 999;
   CK(ddc_find_display_ref_by_selector(dsel) == NULL);
   dsel_free(dsel);
}


static void test_phantom_displays_excluded(void) {
   // dref_phantom shares dref1's EDID identity, but has busno 103 and
   // DISPNO_PHANTOM; searching by its busno must not find it, since
   // ddc_find_display_ref_by_selector() skips DISPNO_PHANTOM entries
   // unconditionally, before criteria matching.
   Display_Selector * dsel = dsel_new();
   dsel->busno = 103;
   CK(ddc_find_display_ref_by_selector(dsel) == NULL);
   dsel_free(dsel);
}


static void test_empty_selector_matches_first_non_phantom(void) {
   // dsel_new() default: no criteria set -> ddc_test_display_ref_by_selector()
   // returns true unconditionally for the first non-phantom entry scanned.
   Display_Selector * dsel = dsel_new();
   CK(ddc_find_display_ref_by_selector(dsel) == dref1);
   dsel_free(dsel);
}


int main(int argc, char ** argv) {
   setup_all_display_refs();

   test_find_by_busno();
   test_find_by_dispno();
   test_find_by_mfg_model_serial();
   test_no_match();
   test_phantom_displays_excluded();
   test_empty_selector_matches_first_non_phantom();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
