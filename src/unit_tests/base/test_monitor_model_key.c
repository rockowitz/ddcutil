/** @file test_monitor_model_key.c
 *
 *  Standalone unit tests for src/base/monitor_model_key.c: constructing a
 *  Monitor_Model_Key value, the defined/undefined predicate, equality, the
 *  model-id string, and the repr.
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

#include "base/monitor_model_key.h"

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
   Monitor_Model_Key mmk = mmk_value("DEL", "U2412M", 4660);
   CK_STR(mmk.mfg_id, "DEL");
   CK_STR(mmk.model_name, "U2412M");
   CK_INT(mmk.product_code, 4660);
   CK(mmk.defined == true);

   Monitor_Model_Key undef = mmk_undefined_value();
   CK(undef.defined == false);

   // equality
   Monitor_Model_Key same  = mmk_value("DEL", "U2412M", 4660);
   Monitor_Model_Key diff  = mmk_value("DEL", "U2412M", 9999);
   CK(monitor_model_key_eq(mmk, same) == true);
   CK(monitor_model_key_eq(mmk, diff) == false);
   CK(monitor_model_key_eq(undef, mmk_undefined_value()) == true);   // both undefined
   CK(monitor_model_key_eq(mmk, undef) == false);                    // defined vs undefined

   // model id string: "mfg-model-product"
   char * mid = mmk_model_id_string("DEL", "U2412M", 4660);
   CK_STR(mid, "DEL-U2412M-4660");
   free(mid);

   // repr
   CK_STR(mmk_repr(mmk), "[DEL,U2412M,4660]");
   CK_STR(mmk_repr(undef), "[Undefined]");

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
