/** @file test_glib_string_util.c
 *
 *  Standalone unit tests for src/util/glib_string_util.c: joining a GPtrArray of
 *  strings (with and without sorting, and the thread-buffer variants), searching
 *  a GPtrArray, and the unique-string-set operations (equality, difference, and
 *  duplicate-suppressing insert).
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
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/glib_string_util.h"

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

// Builds a GPtrArray (free func g_free) from a NULL-terminated argument list.
static GPtrArray * mk(const char * first, ...) {
   GPtrArray * a = g_ptr_array_new_with_free_func(g_free);
   va_list ap;
   va_start(ap, first);
   for (const char * s = first; s != NULL; s = va_arg(ap, const char *))
      g_ptr_array_add(a, g_strdup(s));
   va_end(ap);
   return a;
}

static void test_join(void) {
   GPtrArray * a = mk("x", "y", "z", NULL);
   char * s = join_string_g_ptr_array(a, ",");
   CK_STR(s, "x,y,z");
   free(s);
   s = join_string_g_ptr_array(a, " - ");
   CK_STR(s, "x - y - z");
   free(s);

   // thread-buffer variant yields the same text
   CK_STR(join_string_g_ptr_array_t(a, ","), "x,y,z");
   g_ptr_array_free(a, TRUE);

   // single element: no separator emitted
   GPtrArray * one = mk("solo", NULL);
   s = join_string_g_ptr_array(one, ",");
   CK_STR(s, "solo");
   free(s);
   g_ptr_array_free(one, TRUE);

   // NULL array -> empty string
   s = join_string_g_ptr_array(NULL, ",");
   CK_STR(s, "");
   free(s);
}

static void test_join_sorted(void) {
   GPtrArray * a = mk("cherry", "apple", "banana", NULL);
   // sort=false preserves insertion order
   char * s = join_string_g_ptr_array2(a, ",", false);
   CK_STR(s, "cherry,apple,banana");
   free(s);
   // sort=true orders the array (in place) before joining
   s = join_string_g_ptr_array2(a, ",", true);
   CK_STR(s, "apple,banana,cherry");
   free(s);
   g_ptr_array_free(a, TRUE);
}

static void test_find(void) {
   GPtrArray * a = mk("alpha", "beta", "gamma", NULL);
   CK_INT(gaux_string_ptr_array_find(a, "beta"), 1);
   CK_INT(gaux_string_ptr_array_find(a, "alpha"), 0);
   CK_INT(gaux_string_ptr_array_find(a, "missing"), -1);
   CK_INT(gaux_string_ptr_array_find(a, "Beta"), -1);   // case sensitive
   g_ptr_array_free(a, TRUE);
}

static void test_set_ops(void) {
   GPtrArray * a = mk("a", "b", "c", NULL);
   GPtrArray * b = mk("c", "b", "a", NULL);        // same set, different order
   GPtrArray * c = mk("a", "b", "x", NULL);        // differs
   GPtrArray * d = mk("a", "b", NULL);             // different length

   CK(gaux_unique_string_ptr_arrays_equal(a, b) == true);
   CK(gaux_unique_string_ptr_arrays_equal(a, c) == false);
   CK(gaux_unique_string_ptr_arrays_equal(a, d) == false);

   // difference: a - {b} = {a, c}
   GPtrArray * only_b = mk("b", NULL);
   GPtrArray * diff = gaux_unique_string_ptr_arrays_minus(a, only_b);
   CK_INT(diff->len, 2);
   CK(gaux_string_ptr_array_find(diff, "a") >= 0);
   CK(gaux_string_ptr_array_find(diff, "c") >= 0);
   CK(gaux_string_ptr_array_find(diff, "b") == -1);
   g_ptr_array_free(diff, TRUE);

   // difference with nothing removed keeps all
   GPtrArray * none = mk("z", NULL);
   GPtrArray * diff2 = gaux_unique_string_ptr_arrays_minus(a, none);
   CK_INT(diff2->len, 3);
   g_ptr_array_free(diff2, TRUE);

   g_ptr_array_free(a, TRUE);
   g_ptr_array_free(b, TRUE);
   g_ptr_array_free(c, TRUE);
   g_ptr_array_free(d, TRUE);
   g_ptr_array_free(only_b, TRUE);
   g_ptr_array_free(none, TRUE);
}

static void test_include(void) {
   GPtrArray * a = mk("one", "two", NULL);
   gaux_unique_string_ptr_array_include(a, "three");   // new value appended
   CK_INT(a->len, 3);
   CK(gaux_string_ptr_array_find(a, "three") >= 0);
   gaux_unique_string_ptr_array_include(a, "two");      // already present: no-op
   CK_INT(a->len, 3);
   g_ptr_array_free(a, TRUE);
}

int main(int argc, char ** argv) {
   test_join();
   test_join_sorted();
   test_find();
   test_set_ops();
   test_include();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
