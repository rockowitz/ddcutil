/** @file test_ddc_vcp_version.c
 *
 *  Standalone unit tests for src/ddc/ddc_vcp_version.c:
 *  get_overriding_vcp_version(), get_saved_vcp_version(), and the guarded
 *  (non-hardware-touching) path of get_vcp_version_by_dref()/
 *  get_vcp_version_by_dh().
 *
 *  get_vcp_version_by_dref()/get_vcp_version_by_dh() only perform real DDC
 *  communication (ddc_open_display(), set_vcp_version_xdf_by_dh()) when
 *  get_saved_vcp_version() returns DDCA_VSPEC_UNQUERIED -- i.e. when
 *  neither an overriding version (command line or dynamic feature record)
 *  nor an already-cached dref->vcp_version_xdf is available. Every test
 *  here pre-sets one of those so the hardware path is provably never
 *  taken; set_vcp_version_xdf_by_dh() itself (which always does DDC I/O)
 *  is not tested.
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

#include "base/displays.h"
#include "base/dynamic_features.h"
#include "base/vcp_version.h"

#include "ddc/ddc_vcp_version.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#define CK_VSPEC(actual, expected) do { \
   total++; \
   DDCA_MCCS_Version_Spec _a = (actual); DDCA_MCCS_Version_Spec _e = (expected); \
   if (_a.major != _e.major || _a.minor != _e.minor) { failed++; \
      printf("FAIL  line %-4d  %s -> %d.%d, expected %d.%d\n", __LINE__, #actual, \
             _a.major, _a.minor, _e.major, _e.minor); } \
} while(0)


static void test_get_overriding_vcp_version(void) {
   Display_Ref * dref = create_bus_display_ref(9999);   // nonexistent bus, never opened

   // neither cmdline override nor dfr set
   CK_VSPEC(get_overriding_vcp_version(dref), DDCA_VSPEC_UNQUERIED);

   // dynamic feature record override
   Dynamic_Features_Rec dfr = {0};
   dfr.vspec = DDCA_VSPEC_V30;
   dref->dfr = &dfr;
   CK_VSPEC(get_overriding_vcp_version(dref), DDCA_VSPEC_V30);

   // command line override takes precedence over dfr
   dref->vcp_version_cmdline = DDCA_VSPEC_V21;
   CK_VSPEC(get_overriding_vcp_version(dref), DDCA_VSPEC_V21);

   // free_display_ref() is a no-op except for DREF_TRANSIENT refs (which
   // this isn't); calling it anyway documents correct API usage, but these
   // fabricated Display_Refs are otherwise just left for process exit.
   dref->dfr = NULL;   // don't let free_display_ref() touch our stack dfr
   free_display_ref(dref);
}


static void test_get_saved_vcp_version(void) {
   Display_Ref * dref = create_bus_display_ref(9999);

   // nothing set at all
   CK_VSPEC(get_saved_vcp_version(dref), DDCA_VSPEC_UNQUERIED);

   // falls back to the cached xdf-detected version when no override is set
   dref->vcp_version_xdf = DDCA_VSPEC_V20;
   CK_VSPEC(get_saved_vcp_version(dref), DDCA_VSPEC_V20);

   // an overriding version takes precedence over the cached xdf version
   dref->vcp_version_cmdline = DDCA_VSPEC_V22;
   CK_VSPEC(get_saved_vcp_version(dref), DDCA_VSPEC_V22);

   free_display_ref(dref);
}


static void test_get_vcp_version_by_dref_guarded(void) {
   // dref->vcp_version_xdf already set -> returns immediately without
   // calling ddc_open_display() (which would fail/crash for this
   // nonexistent bus if reached).
   Display_Ref * dref = create_bus_display_ref(9999);
   dref->vcp_version_xdf = DDCA_VSPEC_V21;
   CK_VSPEC(get_vcp_version_by_dref(dref), DDCA_VSPEC_V21);
   free_display_ref(dref);

   // command line override alone is also sufficient to avoid the hardware path
   Display_Ref * dref2 = create_bus_display_ref(9999);
   dref2->vcp_version_cmdline = DDCA_VSPEC_V30;
   CK_VSPEC(get_vcp_version_by_dref(dref2), DDCA_VSPEC_V30);
   free_display_ref(dref2);
}


static void test_get_vcp_version_by_dh_guarded(void) {
   Display_Ref * dref = create_bus_display_ref(9999);
   dref->vcp_version_xdf = DDCA_VSPEC_V22;
   Display_Handle * dh = create_base_display_handle(-1, dref);

   CK_VSPEC(get_vcp_version_by_dh(dh), DDCA_VSPEC_V22);

   free(dh);
   free_display_ref(dref);
}


int main(int argc, char ** argv) {
   test_get_overriding_vcp_version();
   test_get_saved_vcp_version();
   test_get_vcp_version_by_dref_guarded();
   test_get_vcp_version_by_dh_guarded();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
