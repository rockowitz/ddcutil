/** @file test_drm_connector_state.c
 *
 *  Standalone unit test for src/base/drm_connector_state.c.
 *
 *  redetect_drm_connector_states() scans the host's DRM devices, so instead of
 *  relying on hardware the test populates the module's all_card_connector_states
 *  table directly with synthetic records and checks the find_drm_connector_state
 *  matching logic (by connector id, and by connector type + type id).
 *  init_drm_connector_state() is called as a smoke test.
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

#include "util/drm_card_connector_util.h"   // Drm_Connector_Identifier
#include "base/drm_connector_state.h"

// The module's connector-state table (non-static global), which
// find_drm_connector_state() searches.
extern GPtrArray * all_card_connector_states;

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

static Drm_Connector_Identifier cid(int cardno, int connector_id,
                                    int connector_type, int connector_type_id) {
   Drm_Connector_Identifier c = { cardno, connector_id, connector_type, connector_type_id };
   return c;
}

int main(int argc, char ** argv) {
   init_drm_connector_state();     // smoke test: registers RTTI, must not crash

   Drm_Connector_State cs1;
   memset(&cs1, 0, sizeof(cs1));
   cs1.cardno = 0; cs1.connector_id = 42; cs1.connector_type = 11; cs1.connector_type_id = 1;

   Drm_Connector_State cs2;
   memset(&cs2, 0, sizeof(cs2));
   cs2.cardno = 1; cs2.connector_id = 50; cs2.connector_type = 10; cs2.connector_type_id = 2;

   all_card_connector_states = g_ptr_array_new();
   g_ptr_array_add(all_card_connector_states, &cs1);
   g_ptr_array_add(all_card_connector_states, &cs2);

   // match by connector id
   CK(find_drm_connector_state(cid(0, 42, -1, -1)) == &cs1);
   CK(find_drm_connector_state(cid(1, 50, -1, -1)) == &cs2);
   CK(find_drm_connector_state(cid(0, 99, -1, -1)) == NULL);   // no such id
   CK(find_drm_connector_state(cid(5, 42, -1, -1)) == NULL);   // wrong card

   // match by connector type + type id (connector_id < 0)
   CK(find_drm_connector_state(cid(0, -1, 11, 1)) == &cs1);
   CK(find_drm_connector_state(cid(1, -1, 10, 2)) == &cs2);
   CK(find_drm_connector_state(cid(0, -1, 99, 9)) == NULL);    // no such type
   CK(find_drm_connector_state(cid(0, -1, 11, 9)) == NULL);    // type matches, id does not

   g_ptr_array_free(all_card_connector_states, FALSE);   // entries are on the stack
   all_card_connector_states = NULL;

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
