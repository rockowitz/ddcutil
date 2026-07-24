/** @file test_sysfs_sys_drm_connector.c
 *
 *  Standalone unit tests for the search functions in
 *  src/sysfs/sysfs_sys_drm_connector.c.  These search the global
 *  sys_drm_connectors array (normally populated by scanning
 *  /sys/class/drm); this test populates it directly with fabricated
 *  Sys_Drm_Connector records instead, so no real DRM hardware is needed.
 *  Every find_*()/sys_drm_get_busno_by_connector_name()/
 *  all_sys_drm_connectors_have_connector_id()/
 *  buses_having_edid_from_sys_drm_connectors() function checks
 *  `if (!sys_drm_connectors) sys_drm_connectors = scan_sys_drm_connectors(...)`
 *  or takes rescan=false, so pre-populating the global and never passing
 *  rescan=true keeps the real scan from ever running.
 *
 *  Not exercised: scan_sys_drm_connectors()/get_sys_drm_connectors(true)
 *  themselves (the real /sys/class/drm scan), report_sys_drm_connectors()
 *  (reporting only), and i2c_check_businfo_connector() (in a #ifdef UNUSED
 *  block, not built).
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libsysfs unit test: it links the internal
 *  libsysfs/libi2c/libbase/libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/drm_card_connector_util.h"

#include "sysfs/sysfs_sys_drm_connector.h"

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


static Sys_Drm_Connector * make_connector(const char * name, int busno, int connector_id,
                                           const Byte * edid /* 128 bytes or NULL */)
{
   Sys_Drm_Connector * c = calloc(1, sizeof(Sys_Drm_Connector));
   c->connector_name = g_strdup(name);
   c->i2c_busno      = busno;
   c->base_busno     = -1;
   c->connector_id   = connector_id;
   if (edid) {
      c->edid_bytes = malloc(128);
      memcpy(c->edid_bytes, edid, 128);
      c->edid_size  = 128;
   }
   return c;
}


int main(int argc, char ** argv) {
   Byte edid1[128];
   memset(edid1, 0xA1, sizeof(edid1));
   Byte edid2[128];
   memset(edid2, 0xB2, sizeof(edid2));

   Sys_Drm_Connector * c1 = make_connector("card0-DP-1",     3, 10, edid1);
   Sys_Drm_Connector * c2 = make_connector("card0-HDMI-A-1", 5, 11, NULL);   // no EDID: not connected
   Sys_Drm_Connector * c3 = make_connector("card1-DP-1",     7, -1, edid2);  // no connector_id

   sys_drm_connectors = g_ptr_array_new_with_free_func(free_sys_drm_connector);
   g_ptr_array_add(sys_drm_connectors, c1);
   g_ptr_array_add(sys_drm_connectors, c2);
   g_ptr_array_add(sys_drm_connectors, c3);

   // find_sys_drm_connector_by_busno
   CK(find_sys_drm_connector_by_busno(3) == c1);
   CK(find_sys_drm_connector_by_busno(5) == c2);
   CK(find_sys_drm_connector_by_busno(9999) == NULL);

   // find_sys_drm_connector_by_connector_id (stops at first entry with
   // connector_id < 0, matching the real function's early-break semantics)
   CK(find_sys_drm_connector_by_connector_id(10) == c1);
   CK(find_sys_drm_connector_by_connector_id(11) == c2);

   // find_sys_drm_connector_by_connector_name
   CK(find_sys_drm_connector_by_connector_name("card0-DP-1") == c1);
   CK(find_sys_drm_connector_by_connector_name("card1-DP-1") == c3);
   CK(find_sys_drm_connector_by_connector_name("no-such-connector") == NULL);

   // find_sys_drm_connector_by_edid
   CK(find_sys_drm_connector_by_edid(edid1) == c1);
   CK(find_sys_drm_connector_by_edid(edid2) == c3);
   Byte edid_none[128];
   memset(edid_none, 0xFF, sizeof(edid_none));
   CK(find_sys_drm_connector_by_edid(edid_none) == NULL);

   // find_sys_drm_connector: combined search, busno takes priority
   CK(find_sys_drm_connector(3, NULL, NULL) == c1);
   CK(find_sys_drm_connector(-1, edid2, NULL) == c3);
   CK(find_sys_drm_connector(-1, NULL, "card0-HDMI-A-1") == c2);

   // sys_drm_get_busno_by_connector_name
   CK_INT(sys_drm_get_busno_by_connector_name("card0-DP-1"), 3);
   CK_INT(sys_drm_get_busno_by_connector_name("no-such-connector"), -1);

   // find_drm_connector_name_by_busno / get_drm_connector_name_by_edid
   char * name = find_drm_connector_name_by_busno(5);
   CK_STR(name, "card0-HDMI-A-1");
   free(name);
   CK(find_drm_connector_name_by_busno(9999) == NULL);

   name = get_drm_connector_name_by_edid(edid1);
   CK_STR(name, "card0-DP-1");
   free(name);

   // all_sys_drm_connectors_have_connector_id(false): does not rescan
   CK(!all_sys_drm_connectors_have_connector_id(false));   // c3 has connector_id == -1
   c3->connector_id = 12;
   CK(all_sys_drm_connectors_have_connector_id(false));
   c3->connector_id = -1;   // restore

   // buses_having_edid_from_sys_drm_connectors(false): does not rescan
   Bit_Set_256 with_edid = buses_having_edid_from_sys_drm_connectors(false);
   CK(bs256_contains(with_edid, 3));    // c1 has an edid
   CK(!bs256_contains(with_edid, 5));   // c2 does not
   CK(bs256_contains(with_edid, 7));    // c3 has an edid
   CK_INT(bs256_count(with_edid), 2);

   // find_sys_drm_connector_by_connector_identifier, via the real name parser
   Drm_Connector_Identifier dci = parse_sys_drm_connector_name("card0-DP-1");
   CK(find_sys_drm_connector_by_connector_identifier(dci) == c1);

   // free_sys_drm_connector: single-instance lifecycle, independent of the array
   Sys_Drm_Connector * standalone = make_connector("card2-DP-1", 9, 1, edid1);
   free_sys_drm_connector(standalone);
   free_sys_drm_connector(NULL);   // must not crash

   free_sys_drm_connectors();   // frees c1, c2, c3 and the array; resets global to NULL
   CK(sys_drm_connectors == NULL);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
