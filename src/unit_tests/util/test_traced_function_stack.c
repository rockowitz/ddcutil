/** @file test_traced_function_stack.c
 *
 *  Standalone unit tests for the functions in src/util/traced_function_stack.c.
 *
 *  The traced function stack is a per-thread LIFO of function names, maintained
 *  through push_traced_function() / pop_traced_function().  These tests exercise
 *  push/peek/pop, the size accessor, the two orderings returned by
 *  get_current_traced_function_stack_contents(), and the stash/restore
 *  round-trip used to preserve the stack across a nested callback.
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

#include "util/traced_function_stack.h"

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

// Checks that a GPtrArray of strings equals the NULL-terminated expected list.
static void ck_contents(int line, GPtrArray * a, const char * const * expected) {
   int n = 0;
   while (expected[n]) n++;
   total++;
   bool ok = ((int) a->len == n);
   for (int i = 0; ok && i < n; i++)
      ok = (strcmp((char *) g_ptr_array_index(a, i), expected[i]) == 0);
   if (!ok) {
      failed++;
      printf("FAIL  line %-4d  contents ->", line);
      for (guint i = 0; i < a->len; i++) printf(" %s", (char *) g_ptr_array_index(a, i));
      printf(", expected");
      for (int i = 0; i < n; i++) printf(" %s", expected[i]);
      printf("\n");
   }
}
#define CK_CONTENTS(a, ...) \
   ck_contents(__LINE__, (a), (const char * const []){ __VA_ARGS__, NULL })

int main(int argc, char ** argv) {
   traced_function_stack_enabled = true;

   // empty stack: size 0, peek NULL
   CK_INT(current_traced_function_stack_size(), 0);
   CK(peek_traced_function() == NULL);

   // push three: the most recently pushed is on top
   push_traced_function("alpha");
   push_traced_function("beta");
   push_traced_function("gamma");
   CK_INT(current_traced_function_stack_size(), 3);
   CK_STR(peek_traced_function(), "gamma");

   // ordering matches the documented constants
   GPtrArray * last = get_current_traced_function_stack_contents(TFS_MOST_RECENT_LAST);
   CK_CONTENTS(last, "alpha", "beta", "gamma");    // most recent last
   g_ptr_array_free(last, true);

   GPtrArray * first = get_current_traced_function_stack_contents(TFS_MOST_RECENT_FIRST);
   CK_CONTENTS(first, "gamma", "beta", "alpha");    // most recent first
   g_ptr_array_free(first, true);

   // pop removes the top entry
   pop_traced_function("gamma");
   CK_INT(current_traced_function_stack_size(), 2);
   CK_STR(peek_traced_function(), "beta");
   pop_traced_function("beta");
   pop_traced_function("alpha");
   CK_INT(current_traced_function_stack_size(), 0);
   CK(peek_traced_function() == NULL);

   // stash / restore round-trip preserves order (does not reverse the stack)
   push_traced_function("one");
   push_traced_function("two");
   push_traced_function("three");
   GPtrArray * stashed = stash_current_traced_function_stack();
   reset_current_traced_function_stack();
   CK_INT(current_traced_function_stack_size(), 0);
   restore_current_traced_function_stack(stashed);   // consumes stashed
   CK_INT(current_traced_function_stack_size(), 3);
   CK_STR(peek_traced_function(), "three");           // top unchanged after round-trip
   GPtrArray * after = get_current_traced_function_stack_contents(TFS_MOST_RECENT_LAST);
   CK_CONTENTS(after, "one", "two", "three");
   g_ptr_array_free(after, true);

   // reset empties the stack
   reset_current_traced_function_stack();
   CK_INT(current_traced_function_stack_size(), 0);

   // while disabled, push is a no-op
   traced_function_stack_enabled = false;
   push_traced_function("ignored");
   CK_INT(current_traced_function_stack_size(), 0);
   traced_function_stack_enabled = true;

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
