/** @file test_parsed_capabilities_feature.c
 *
 *  Standalone unit tests for src/vcp/parsed_capabilities_feature.c:
 *  parse_capabilities_feature() and free_capabilities_feature_record().
 *  Pure string parsing of the value list inside a single vcp() feature
 *  segment of a capabilities string -- no hardware or file I/O.
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

#include "util/data_structures.h"

#include "vcp/parsed_capabilities_feature.h"

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


static void test_no_values(void) {
   GPtrArray * errmsgs = g_ptr_array_new_with_free_func(g_free);
   Capabilities_Feature_Record * vfr = parse_capabilities_feature(0x10, NULL, 0, errmsgs);
   CK(vfr != NULL);
   if (vfr) {
      CK_INT(vfr->feature_id, 0x10);
      CK(vfr->value_string == NULL);
      CK(vfr->values == NULL);
      CK_INT(vfr->valid_values, false);   // never set, calloc'd to 0
      free_capabilities_feature_record(vfr);
   }
   CK_INT(errmsgs->len, 0);
   g_ptr_array_free(errmsgs, true);
}


static void test_valid_value_list(void) {
   GPtrArray * errmsgs = g_ptr_array_new_with_free_func(g_free);
   char * s = "01 02 0A";
   Capabilities_Feature_Record * vfr = parse_capabilities_feature(0x60, s, strlen(s), errmsgs);
   CK(vfr != NULL);
   if (vfr) {
      CK_INT(vfr->feature_id, 0x60);
      CK_STR(vfr->value_string, "01 02 0A");
      CK(vfr->valid_values);
      CK(vfr->values != NULL);
      if (vfr->values) {
         CK_INT(bva_length(vfr->values), 3);
         CK(bva_contains(vfr->values, 0x01));
         CK(bva_contains(vfr->values, 0x02));
         CK(bva_contains(vfr->values, 0x0a));
         CK(!bva_contains(vfr->values, 0x03));
      }
      free_capabilities_feature_record(vfr);
   }
   CK_INT(errmsgs->len, 0);
   g_ptr_array_free(errmsgs, true);
}


static void test_invalid_value_list(void) {
   GPtrArray * errmsgs = g_ptr_array_new_with_free_func(g_free);
   char * s = "zz";   // not valid hex
   Capabilities_Feature_Record * vfr = parse_capabilities_feature(0x60, s, strlen(s), errmsgs);
   CK(vfr != NULL);
   if (vfr) {
      CK(!vfr->valid_values);
      free_capabilities_feature_record(vfr);
   }
   CK(errmsgs->len > 0);
   g_ptr_array_free(errmsgs, true);
}


static void test_free_null_safe(void) {
   free_capabilities_feature_record(NULL);   // must not crash
   CK(true);
}


int main(int argc, char ** argv) {
   test_no_values();
   test_valid_value_list();
   test_invalid_value_list();
   test_free_null_safe();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
