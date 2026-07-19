/** @file test_rtti.c
 *
 *  Standalone unit tests for src/base/rtti.c: registering functions in the
 *  name/address table and looking them up by name and by address.
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

#include "base/rtti.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)

static void sample_func_one(void) {}
static void sample_func_two(void) {}

int main(int argc, char ** argv) {
   rtti_func_name_table_add((void *) sample_func_one, "sample_func_one");
   rtti_func_name_table_add((void *) sample_func_two, "sample_func_two");

   // lookup by name -> address
   CK(rtti_get_func_addr_by_name("sample_func_one") == (void *) sample_func_one);
   CK(rtti_get_func_addr_by_name("sample_func_two") == (void *) sample_func_two);
   CK(rtti_get_func_addr_by_name("no_such_function") == NULL);

   // lookup by address -> name
   CK_STR(rtti_get_func_name_by_addr((void *) sample_func_one), "sample_func_one");
   CK_STR(rtti_get_func_name_by_addr((void *) sample_func_two), "sample_func_two");
   CK_STR(rtti_get_func_name_by_addr((void *) main), "<Not Found>");   // unregistered address

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
