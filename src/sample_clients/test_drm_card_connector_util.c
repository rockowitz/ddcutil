/** @file test_drm_card_connector_util.c
 *
 *  Standalone unit tests for the pure functions in
 *  src/util/drm_card_connector_util.c: parsing a sysfs DRM connector name into a
 *  Drm_Connector_Identifier, the identifier equality/compare operators, the
 *  connector-name comparison used for natural sorting, and the repr functions.
 *
 *  The functions that scan /sys (get_sysfs_drm_card_numbers,
 *  check_all_video_adapters_implement_drm) depend on the host's hardware and are
 *  not exercised here.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  Not a libddcutil client -- it links the internal util convenience library
 *  directly, since these symbols are not exported from libddcutil.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/drm_card_connector_util.h"

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

static Drm_Connector_Identifier dci(int cardno, int connector_id,
                                    int connector_type, int connector_type_id) {
   Drm_Connector_Identifier d = { cardno, connector_id, connector_type, connector_type_id };
   return d;
}

static void test_dci_eq(void) {
   // a positive matching connector_id makes two identifiers equal regardless
   // of the other fields
   CK(dci_eq(dci(1, 5, 10, 2), dci(9, 5, 99, 99)) == true);

   // connector_id <= 0: equality requires cardno, type, and type_id to match
   CK(dci_eq(dci(1, 0, 10, 2), dci(1, 0, 10, 2)) == true);
   CK(dci_eq(dci(1, 0, 10, 2), dci(1, 0, 10, 3)) == false);   // type_id differs
   CK(dci_eq(dci(1, 0, 10, 2), dci(2, 0, 10, 2)) == false);   // cardno differs
   CK(dci_eq(dci(1, 0, 10, 2), dci(1, 0, 11, 2)) == false);   // type differs

   // connector_id of 0 does not trigger the id-match shortcut
   CK(dci_eq(dci(1, 0, 10, 2), dci(2, 0, 11, 3)) == false);
}

static void test_dci_cmp(void) {
   // ordered by cardno, then connector_type, then connector_type_id
   CK_INT(dci_cmp(dci(0, -1, 5, 2), dci(1, -1, 5, 2)), -1);
   CK_INT(dci_cmp(dci(1, -1, 5, 2), dci(0, -1, 5, 2)),  1);
   CK_INT(dci_cmp(dci(1, -1, 5, 2), dci(1, -1, 7, 2)), -1);   // type
   CK_INT(dci_cmp(dci(1, -1, 5, 2), dci(1, -1, 5, 9)), -1);   // type_id
   CK_INT(dci_cmp(dci(1, -1, 5, 2), dci(1, -1, 5, 2)),  0);   // equal
}

static void test_parse(void) {
   Drm_Connector_Identifier d = parse_sys_drm_connector_name("card0-DP-1");
   CK_INT(d.cardno, 0);
   CK_INT(d.connector_type_id, 1);
   CK_INT(d.connector_id, -1);            // not set by the name parse

   d = parse_sys_drm_connector_name("card1-HDMI-A-3");
   CK_INT(d.cardno, 1);
   CK_INT(d.connector_type_id, 3);

   // unparseable name yields the all-(-1) sentinel
   d = parse_sys_drm_connector_name("not-a-connector");
   CK_INT(d.cardno, -1);
   CK_INT(d.connector_type_id, -1);
}

static void test_name_cmp(void) {
   // NULL handling
   CK_INT(sys_drm_connector_name_cmp0(NULL, "card0-DP-1"), -1);
   CK_INT(sys_drm_connector_name_cmp0(NULL, NULL), 0);
   CK_INT(sys_drm_connector_name_cmp0("card0-DP-1", NULL), 1);

   // same card and type, ordered numerically by the trailing id
   CK(sys_drm_connector_name_cmp0("card0-DP-1", "card0-DP-2") < 0);
   CK(sys_drm_connector_name_cmp0("card1-DP-10", "card1-DP-2") > 0);   // 10 after 2
   CK_INT(sys_drm_connector_name_cmp0("card0-DP-1", "card0-DP-1"), 0);

   // lower card number sorts first
   CK(sys_drm_connector_name_cmp0("card0-DP-9", "card1-DP-1") < 0);
}

static void test_repr(void) {
   Drm_Connector_Identifier d = dci(2, -1, -1, 7);
   char * r = dci_repr(d);
   CK(strstr(r, "cardno=2") != NULL);
   CK(strstr(r, "connector_type_id=7") != NULL);

   // the thread-buffer variant yields the same text
   char * rt = dci_repr_t(d);
   CK(strcmp(r, rt) == 0);
   free(r);
}

int main(int argc, char ** argv) {
   test_dci_eq();
   test_dci_cmp();
   test_parse();
   test_name_cmp();
   test_repr();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
