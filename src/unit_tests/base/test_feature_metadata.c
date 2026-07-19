/** @file test_feature_metadata.c
 *
 *  Standalone unit tests for the SL value-table helpers in
 *  src/base/feature_metadata.c: lookup by value id, and the deep copy.
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

#include "public/ddcutil_types.h"
#include "base/feature_metadata.h"

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
   DDCA_Feature_Value_Entry table[] = {
      { 0x01, "On" },
      { 0x02, "Off" },
      { 0x00, NULL },     // terminator
   };

   // lookup by value id
   CK_STR(sl_value_table_lookup(table, 0x01), "On");
   CK_STR(sl_value_table_lookup(table, 0x02), "Off");
   CK(sl_value_table_lookup(table, 0x03) == NULL);   // not present

   // deep copy: same content, distinct name storage
   DDCA_Feature_Value_Entry * copy = copy_sl_value_table(table);
   CK(copy != NULL);
   CK_INT(copy[0].value_code, 0x01);
   CK_STR(copy[0].value_name, "On");
   CK(copy[0].value_name != table[0].value_name);   // strings are duplicated
   CK_STR(copy[1].value_name, "Off");
   CK(copy[2].value_name == NULL);                   // terminator preserved
   free_sl_value_table(copy);

   CK(copy_sl_value_table(NULL) == NULL);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
