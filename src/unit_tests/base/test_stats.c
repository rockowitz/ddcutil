/** @file test_stats.c
 *
 *  Standalone unit tests for src/base/stats.c: the retry-operation name and
 *  description lookups.
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

#include "base/stats.h"

static int total = 0;
static int failed = 0;

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)

int main(int argc, char ** argv) {
   CK_STR(retry_type_name(WRITE_ONLY_TRIES_OP),  "WRITE_ONLY_TRIES_OP");
   CK_STR(retry_type_name(WRITE_READ_TRIES_OP),  "WRITE_READ_TRIES_OP");
   CK_STR(retry_type_name(MULTI_PART_READ_OP),   "MULTI_PART_READ_OP");
   CK_STR(retry_type_name(MULTI_PART_WRITE_OP),  "MULTI_PART_WRITE_OP");

   CK_STR(retry_type_description(WRITE_ONLY_TRIES_OP), "write only");
   CK_STR(retry_type_description(WRITE_READ_TRIES_OP), "write-read");
   CK_STR(retry_type_description(MULTI_PART_READ_OP),  "multi-part read");
   CK_STR(retry_type_description(MULTI_PART_WRITE_OP), "multi-part write");

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
