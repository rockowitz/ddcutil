/** @file test_vcp_feature_values.c
 *
 *  Standalone unit tests for src/vcp/vcp_feature_values.c: the
 *  DDCA_Any_Vcp_Value constructors (nontable, continuous, table by
 *  bytes/buffer, by parsed vcp response), summarize_single_vcp_value_r(),
 *  vcp_value_type_name()/vcp_value_type_id(), and the Vcp_Value_Set
 *  (GPtrArray wrapper) lifecycle.  All functions here are pure data
 *  construction/formatting -- no hardware or file I/O.
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

#include "base/ddc_packets.h"

#include "vcp/vcp_feature_values.h"

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


static void test_vcp_value_type_name_and_id(void) {
   CK_STR(vcp_value_type_name(DDCA_NON_TABLE_VCP_VALUE), "Non Table");
   CK_STR(vcp_value_type_name(DDCA_TABLE_VCP_VALUE),     "Table");
   CK_STR(vcp_value_type_id(DDCA_NON_TABLE_VCP_VALUE),   "DDCA_NON_TABLE_VCP_VALUE");
   CK_STR(vcp_value_type_id(DDCA_TABLE_VCP_VALUE),       "DDCA_TABLE_VCP_VALUE");
}


static void test_create_nontable_vcp_value(void) {
   DDCA_Any_Vcp_Value * v = create_nontable_vcp_value(0x10, 0x00, 0x64, 0x00, 0x32);
   CK(v != NULL);
   if (v) {
      CK_INT(v->opcode, 0x10);
      CK(v->value_type == DDCA_NON_TABLE_VCP_VALUE);
      CK_INT(v->val.c_nc.mh, 0x00);
      CK_INT(v->val.c_nc.ml, 0x64);
      CK_INT(v->val.c_nc.sh, 0x00);
      CK_INT(v->val.c_nc.sl, 0x32);
      CK_INT(VALREC_MAX_VAL(v), 0x64);
      CK_INT(VALREC_CUR_VAL(v), 0x32);
      free_single_vcp_value(v);
   }
}


static void test_create_cont_vcp_value(void) {
   DDCA_Any_Vcp_Value * v = create_cont_vcp_value(0x10, 100, 50);
   CK(v != NULL);
   if (v) {
      CK_INT(VALREC_MAX_VAL(v), 100);
      CK_INT(VALREC_CUR_VAL(v), 50);
      free_single_vcp_value(v);
   }

   // values spanning both bytes
   DDCA_Any_Vcp_Value * v2 = create_cont_vcp_value(0x62, 0x1234, 0x5678);
   CK(v2 != NULL);
   if (v2) {
      CK_INT(VALREC_MAX_VAL(v2), 0x1234);
      CK_INT(VALREC_CUR_VAL(v2), 0x5678);
      free_single_vcp_value(v2);
   }
}


static void test_create_table_vcp_value(void) {
   Byte bytes[] = {0x01, 0x02, 0x03, 0x04};

   DDCA_Any_Vcp_Value * v = create_table_vcp_value_by_bytes(0x72, bytes, sizeof(bytes));
   CK(v != NULL);
   if (v) {
      CK(v->value_type == DDCA_TABLE_VCP_VALUE);
      CK_INT(v->val.t.bytect, 4);
      CK(memcmp(v->val.t.bytes, bytes, 4) == 0);
      // bytes must be a copy, not an alias
      CK(v->val.t.bytes != bytes);
      free_single_vcp_value(v);
   }

   Buffer * buf = buffer_new(10, __func__);
   buffer_put(buf, bytes, sizeof(bytes));
   DDCA_Any_Vcp_Value * v2 = create_table_vcp_value_by_buffer(0x72, buf);
   CK(v2 != NULL);
   if (v2) {
      CK_INT(v2->val.t.bytect, 4);
      CK(memcmp(v2->val.t.bytes, bytes, 4) == 0);
      free_single_vcp_value(v2);
   }
   buffer_free(buf, __func__);
}


static void test_create_single_vcp_value_by_parsed_vcp_response(void) {
   // Non-table response
   Parsed_Nontable_Vcp_Response ntresp = {0};
   ntresp.vcp_code = 0x10;
   ntresp.valid_response = true;
   ntresp.supported_opcode = true;
   ntresp.mh = 0x00;
   ntresp.ml = 0x64;
   ntresp.sh = 0x00;
   ntresp.sl = 0x32;

   Parsed_Vcp_Response presp = {0};
   presp.response_type = DDCA_NON_TABLE_VCP_VALUE;
   presp.non_table_response = &ntresp;

   DDCA_Any_Vcp_Value * v = create_single_vcp_value_by_parsed_vcp_response(0x10, &presp);
   CK(v != NULL);
   if (v) {
      CK_INT(v->opcode, 0x10);
      CK_INT(VALREC_MAX_VAL(v), 0x64);
      CK_INT(VALREC_CUR_VAL(v), 0x32);
      free_single_vcp_value(v);
   }

   // Table response
   Byte bytes[] = {0xAA, 0xBB};
   Buffer * buf = buffer_new(10, __func__);
   buffer_put(buf, bytes, sizeof(bytes));

   Parsed_Vcp_Response tpresp = {0};
   tpresp.response_type = DDCA_TABLE_VCP_VALUE;
   tpresp.table_response = buf;

   DDCA_Any_Vcp_Value * tv = create_single_vcp_value_by_parsed_vcp_response(0x72, &tpresp);
   CK(tv != NULL);
   if (tv) {
      CK(tv->value_type == DDCA_TABLE_VCP_VALUE);
      CK_INT(tv->val.t.bytect, 2);
      CK(memcmp(tv->val.t.bytes, bytes, 2) == 0);
      free_single_vcp_value(tv);
   }
   buffer_free(buf, __func__);
}


static void test_single_vcp_value_to_nontable_vcp_value(void) {
   DDCA_Any_Vcp_Value * v = create_nontable_vcp_value(0x10, 0x00, 0x64, 0x00, 0x32);
   Nontable_Vcp_Value * nv = single_vcp_value_to_nontable_vcp_value(v);
   CK(nv != NULL);
   if (nv) {
      CK_INT(nv->vcp_code, 0x10);
      CK_INT(nv->max_value, 0x64);
      CK_INT(nv->cur_value, 0x32);
      CK_INT(nv->mh, 0x00);
      CK_INT(nv->ml, 0x64);
      CK_INT(nv->sh, 0x00);
      CK_INT(nv->sl, 0x32);
      free(nv);
   }
   free_single_vcp_value(v);
}


static void test_summarize_single_vcp_value(void) {
   DDCA_Any_Vcp_Value * v = create_nontable_vcp_value(0x10, 0x00, 0x64, 0x00, 0x32);
   char * s = summarize_single_vcp_value(v);
   CK(s != NULL);
   CK_STR_CONTAINS(s, "opcode=0x10");
   CK_STR_CONTAINS(s, "max_val=100");
   CK_STR_CONTAINS(s, "cur_val=50");
   free_single_vcp_value(v);

   Byte bytes[] = {0x01, 0x02, 0x03};
   DDCA_Any_Vcp_Value * tv = create_table_vcp_value_by_bytes(0x72, bytes, sizeof(bytes));
   char * ts = summarize_single_vcp_value(tv);
   CK(ts != NULL);
   CK_STR_CONTAINS(ts, "opcode=0x72");
   CK_STR_CONTAINS(ts, "Table");
   free_single_vcp_value(tv);
}


static void test_free_single_vcp_value_null_safe(void) {
   free_single_vcp_value(NULL);   // must not crash
   CK(true);
}


static void test_vcp_value_set(void) {
   Vcp_Value_Set vset = vcp_value_set_new(4);
   CK(vset != NULL);
   CK_INT(vcp_value_set_size(vset), 0);

   DDCA_Any_Vcp_Value * v1 = create_nontable_vcp_value(0x10, 0, 100, 0, 50);
   DDCA_Any_Vcp_Value * v2 = create_nontable_vcp_value(0x12, 0, 100, 0, 75);
   vcp_value_set_add(vset, v1);
   vcp_value_set_add(vset, v2);
   CK_INT(vcp_value_set_size(vset), 2);

   DDCA_Any_Vcp_Value * got0 = vcp_value_set_get(vset, 0);
   CK(got0 == v1);
   CK_INT(got0->opcode, 0x10);
   DDCA_Any_Vcp_Value * got1 = vcp_value_set_get(vset, 1);
   CK(got1 == v2);
   CK_INT(got1->opcode, 0x12);

   // free_vcp_value_set() invokes free_single_vcp_value() on each entry via
   // the free_func installed in vcp_value_set_new(); must not crash or leak.
   free_vcp_value_set(vset);
   CK(true);
}


int main(int argc, char ** argv) {
   test_vcp_value_type_name_and_id();
   test_create_nontable_vcp_value();
   test_create_cont_vcp_value();
   test_create_table_vcp_value();
   test_create_single_vcp_value_by_parsed_vcp_response();
   test_single_vcp_value_to_nontable_vcp_value();
   test_summarize_single_vcp_value();
   test_free_single_vcp_value_null_safe();
   test_vcp_value_set();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
