/** @file test_api_feature_access.c
 *
 *  Standalone unit tests for src/libmain/api_feature_access.c:
 *  ddca_free_table_vcp_value(), ddca_free_any_vcp_value(), and a smoke
 *  test of dbgrpt_any_vcp_value() (unpublished, declared in
 *  api_feature_access_internal.h).
 *
 *  Every other function in this file requires an open display (a
 *  DDCA_Display_Handle from a real or at least validated connection) or
 *  is wrapped in the API_PROLOG/API_PROLOGX macros, which perform full
 *  implicit library initialization (including real I2C bus/display
 *  detection) the first time any such function is called if the library
 *  isn't already initialized -- exactly the kind of real-hardware side
 *  effect these unit tests are designed to avoid.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon+libsharedlib unit test: it links the internal
 *  libmain/libsharedlib.la convenience library (the intermediate library
 *  that becomes libddcutil.so) together with the top-level libcommon
 *  convenience library it depends on.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "public/ddcutil_c_api.h"
#include "public/ddcutil_types.h"

#include "util/coredefs.h"

#include "libmain/api_feature_access_internal.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
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


static void test_free_table_vcp_value(void) {
   DDCA_Table_Vcp_Value * tv = calloc(1, sizeof(DDCA_Table_Vcp_Value));
   tv->bytect = 3;
   tv->bytes = malloc(3);

   ddca_free_table_vcp_value(tv);   // must not crash
   CK(true);
}


static void test_free_table_vcp_value_null_safe(void) {
   ddca_free_table_vcp_value(NULL);   // must not crash
   CK(true);
}


static void test_free_any_vcp_value_non_table(void) {
   DDCA_Any_Vcp_Value * v = calloc(1, sizeof(DDCA_Any_Vcp_Value));
   v->opcode = 0x10;
   v->value_type = DDCA_NON_TABLE_VCP_VALUE;
   v->val.c_nc.sl = 50;

   ddca_free_any_vcp_value(v);   // must not crash; no val.t.bytes to free
   CK(true);
}


static void test_free_any_vcp_value_table(void) {
   DDCA_Any_Vcp_Value * v = calloc(1, sizeof(DDCA_Any_Vcp_Value));
   v->opcode = 0x72;
   v->value_type = DDCA_TABLE_VCP_VALUE;
   v->val.t.bytect = 4;
   v->val.t.bytes = malloc(4);

   ddca_free_any_vcp_value(v);   // must not crash; frees val.t.bytes too
   CK(true);
}


static void test_free_any_vcp_value_null_safe(void) {
   ddca_free_any_vcp_value(NULL);   // must not crash
   CK(true);
}


static void test_dbgrpt_any_vcp_value_smoke(void) {
   DDCA_Any_Vcp_Value nontable = {0};
   nontable.opcode = 0x10;
   nontable.value_type = DDCA_NON_TABLE_VCP_VALUE;
   nontable.val.c_nc.sl = 50;
   nontable.val.c_nc.ml = 100;
   QUIETLY( dbgrpt_any_vcp_value(&nontable, 0) );

   DDCA_Any_Vcp_Value table = {0};
   table.opcode = 0x72;
   table.value_type = DDCA_TABLE_VCP_VALUE;
   Byte bytes[] = {0x01, 0x02, 0x03};
   table.val.t.bytes = bytes;
   table.val.t.bytect = 3;
   QUIETLY( dbgrpt_any_vcp_value(&table, 0) );

   CK(true);   // reaching here without crashing is the test
}


int main(int argc, char ** argv) {
   test_free_table_vcp_value();
   test_free_table_vcp_value_null_safe();
   test_free_any_vcp_value_non_table();
   test_free_any_vcp_value_table();
   test_free_any_vcp_value_null_safe();
   test_dbgrpt_any_vcp_value_smoke();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
