/** @file test_multi_level_map.c
 *
 *  Standalone unit tests for src/util/multi_level_map.c: building a Multi_Level_Map
 *  with mlm_create/mlm_add_node and querying it with mlm_get_names2 and the
 *  varargs mlm_get_names, including partial and failed lookups.
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

#include "util/multi_level_map.h"

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

int main(int argc, char ** argv) {
   MLM_Level levels[2] = {
      { "vendor", 10, 0, NULL },
      { "device", 10, 0, NULL },
   };
   Multi_Level_Map * mlm = mlm_create("test", 2, levels);
   CK(mlm != NULL);
   CK_INT(mlm->levels, 2);

   MLM_Node * v1 = mlm_add_node(mlm, NULL, 0x1000, "Vendor1");
   mlm_add_node(mlm, v1, 0x0001, "Device1");
   mlm_add_node(mlm, v1, 0x0002, "Device2");
   MLM_Node * v2 = mlm_add_node(mlm, NULL, 0x2000, "Vendor2");
   mlm_add_node(mlm, v2, 0x0001, "OtherDevice1");

   // per-level entry counts are tracked
   CK_INT(mlm->level_detail[0].total_entries, 2);   // 2 vendors
   CK_INT(mlm->level_detail[1].total_entries, 3);   // 3 devices

   // one-level lookup
   guint ids1[] = { 0x1000 };
   Multi_Level_Names n1 = mlm_get_names2(mlm, 1, ids1);
   CK_INT(n1.levels, 1);
   CK_STR(n1.names[0], "Vendor1");

   // two-level lookup resolves both names
   guint ids2[] = { 0x1000, 0x0002 };
   Multi_Level_Names n2 = mlm_get_names2(mlm, 2, ids2);
   CK_INT(n2.levels, 2);
   CK_STR(n2.names[0], "Vendor1");
   CK_STR(n2.names[1], "Device2");

   // the same device id under a different vendor
   guint ids3[] = { 0x2000, 0x0001 };
   Multi_Level_Names n3 = mlm_get_names2(mlm, 2, ids3);
   CK_INT(n3.levels, 2);
   CK_STR(n3.names[1], "OtherDevice1");

   // partial match: vendor found, device not -> stops at level 1
   guint ids4[] = { 0x1000, 0x9999 };
   Multi_Level_Names n4 = mlm_get_names2(mlm, 2, ids4);
   CK_INT(n4.levels, 1);
   CK_STR(n4.names[0], "Vendor1");

   // vendor not found -> zero levels
   guint ids5[] = { 0x9999 };
   Multi_Level_Names n5 = mlm_get_names2(mlm, 1, ids5);
   CK_INT(n5.levels, 0);

   // varargs variant matches mlm_get_names2
   Multi_Level_Names n6 = mlm_get_names(mlm, 2, 0x1000, 0x0001);
   CK_INT(n6.levels, 2);
   CK_STR(n6.names[0], "Vendor1");
   CK_STR(n6.names[1], "Device1");

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
