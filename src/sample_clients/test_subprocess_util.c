/** @file test_subprocess_util.c
 *
 *  Standalone unit tests for src/util/subprocess_util.c: running a shell command
 *  and collecting its output, the single-line convenience wrapper, and the
 *  command-in-path check.  Uses portable commands (echo, printf, which) so the
 *  output is deterministic.
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

#include "util/subprocess_util.h"

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
   // single-line result is the first line of output
   char * s = execute_shell_cmd_one_line_result("echo hello");
   CK_STR(s, "hello");
   free(s);

   s = execute_shell_cmd_one_line_result("printf 'first\\nsecond\\n'");
   CK_STR(s, "first");
   free(s);

   // collect returns one array entry per output line
   GPtrArray * lines = execute_shell_cmd_collect("printf 'a\\nb\\nc\\n'");
   CK(lines != NULL);
   CK_INT(lines->len, 3);
   CK_STR((char *) g_ptr_array_index(lines, 0), "a");
   CK_STR((char *) g_ptr_array_index(lines, 2), "c");
   g_ptr_array_free(lines, TRUE);

   // command-in-path check
   CK(is_command_in_path("sh") == true);
   CK(is_command_in_path("no_such_command_zqx_999") == false);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
