/** @file test_backtrace.c
 *
 *  Standalone unit tests for src/util/backtrace.c.
 *
 *  get_backtrace() captures the current call stack as an array of function
 *  names.  The names it produces depend on backtrace_symbols() resolving
 *  symbols (which needs -rdynamic and is not reliable here), so the tests check
 *  structural properties instead: the array is non-NULL with valid entries, the
 *  stack_adjust argument suppresses exactly that many leading frames, and a huge
 *  adjustment yields an empty array.  show_backtrace() output is checked for its
 *  header line.  On a platform without execinfo.h, get_backtrace() returns NULL
 *  and the checks are skipped.
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

#include "util/backtrace.h"
#include "util/report_util.h"

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

static void free_bt(GPtrArray * bt) {
   g_ptr_array_set_free_func(bt, g_free);   // entries are malloc'd names
   g_ptr_array_free(bt, TRUE);
}

int main(int argc, char ** argv) {
   GPtrArray * b0 = get_backtrace(0);
   if (b0 == NULL) {
      printf("NOTE  execinfo unavailable; backtrace checks skipped\n");
   }
   else {
      // non-empty, and every entry is a non-NULL string
      CK(b0->len >= 1);
      bool all_set = true;
      for (guint i = 0; i < b0->len; i++)
         if (g_ptr_array_index(b0, i) == NULL) { all_set = false; break; }
      CK(all_set == true);

      // stack_adjust suppresses that many leading frames: one more suppressed
      // frame => one fewer entry (independent of symbol resolution)
      GPtrArray * b1 = get_backtrace(1);
      CK(b1 != NULL);
      CK_INT(b1->len, b0->len - 1);
      free_bt(b1);

      // an adjustment larger than the stack depth yields an empty (non-NULL) array
      GPtrArray * bbig = get_backtrace(1000);
      CK(bbig != NULL);
      CK_INT(bbig->len, 0);
      free_bt(bbig);

      free_bt(b0);

      // show_backtrace writes a labeled call stack to the report destination
      char * cap = NULL; size_t sz = 0;
      FILE * ms = open_memstream(&cap, &sz);
      rpt_push_output_dest(ms);
      show_backtrace(0);
      fflush(ms);
      rpt_pop_output_dest();
      fclose(ms);
      CK(cap != NULL && strstr(cap, "Current call stack") != NULL);
      g_free(cap);
   }

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
