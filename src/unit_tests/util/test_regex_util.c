/** @file test_regex_util.c
 *
 *  Standalone unit tests for src/util/regex_util.c: compiling/caching an
 *  extended regular expression and evaluating it against a string, both the
 *  plain match test and the variant that returns capture-group offsets.
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
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/regex_util.h"

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

// Copies capture group `n` of `value` (per `m`) into `out`.
static void group(const char * value, regmatch_t * m, int n, char * out) {
   int len = m[n].rm_eo - m[n].rm_so;
   memcpy(out, value + m[n].rm_so, len);
   out[len] = '\0';
}

int main(int argc, char ** argv) {
   // anchored match
   CK(compile_and_eval_regex("^abc", "abcdef") == true);
   CK(compile_and_eval_regex("^abc", "xabcdef") == false);

   // unanchored: matches a substring anywhere
   CK(compile_and_eval_regex("[0-9]+", "abc123") == true);
   CK(compile_and_eval_regex("[0-9]+", "no digits") == false);

   // full-string anchors and extended syntax
   CK(compile_and_eval_regex("^[0-9]+$", "12345") == true);
   CK(compile_and_eval_regex("^[0-9]+$", "12a45") == false);
   CK(compile_and_eval_regex("^(cat|dog)$", "dog") == true);
   CK(compile_and_eval_regex("^(cat|dog)$", "fish") == false);

   // re-using a cached pattern still works
   CK(compile_and_eval_regex("^abc", "abcXYZ") == true);

   // capture groups
   regmatch_t m[3];
   bool ok = compile_and_eval_regex_with_matches("card([0-9]+)-(.*)", "card3-DP", 3, m);
   CK(ok == true);
   char buf[32];
   group("card3-DP", m, 0, buf);  CK_STR(buf, "card3-DP");   // whole match
   group("card3-DP", m, 1, buf);  CK_STR(buf, "3");           // first group
   group("card3-DP", m, 2, buf);  CK_STR(buf, "DP");          // second group

   // no match -> false
   CK(compile_and_eval_regex_with_matches("^card([0-9]+)$", "nope", 3, m) == false);

   free_regex_hash_table();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
