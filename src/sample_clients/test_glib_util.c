/** @file test_glib_util.c
 *
 *  Standalone unit tests for the functions in src/util/glib_util.c.
 *
 *  Checks that each function produces the expected output for normal inputs
 *  and edge cases.  Prints one line per failing check and a summary; exit
 *  status is 0 if all checks pass, 1 otherwise.
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

#include "util/glib_util.h"

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

// GAuxDupFunc that duplicates a string
static gpointer dup_string(gpointer s) { return g_strdup((char *) s); }

// true iff pa holds exactly the expct expected strings, in order
static bool pa_is(GPtrArray * pa, const char ** expected, int expct) {
   if ((int) pa->len != expct)
      return false;
   for (int i = 0; i < expct; i++) {
      if (strcmp((char *) g_ptr_array_index(pa, i), expected[i]) != 0)
         return false;
   }
   return true;
}

static void test_g_list_to_g_array(void) {
   GList * l = NULL;
   l = g_list_append(l, "a");
   l = g_list_append(l, "b");
   l = g_list_append(l, "c");
   guint len = 99;
   gpointer * arr = g_list_to_g_array(l, &len);
   CK_INT(len, 3);
   CK(strcmp((char *) arr[0], "a") == 0);
   CK(strcmp((char *) arr[2], "c") == 0);
   CK(arr[3] == NULL);          // null terminated
   g_free(arr);
   g_list_free(l);

   guint len0 = 99;
   gpointer * arr0 = g_list_to_g_array(NULL, &len0);
   CK_INT(len0, 0);
   CK(arr0[0] == NULL);
   g_free(arr0);
}

static void test_comparators(void) {
   char * s_abc = "abc";
   char * s_ABC = "ABC";
   char * s_abd = "abd";
   // gaux_ptr_scomp: case-insensitive, GCompareFunc over pointers-to-strings
   CK(gaux_ptr_scomp(&s_abc, &s_ABC) == 0);      // differ only in case
   CK(gaux_ptr_scomp(&s_abc, &s_abd) < 0);
   CK(gaux_ptr_scomp(&s_abd, &s_abc) > 0);

   // gaux_ptr_intcomp: normalizes to exactly -1 / 0 / 1
   CK_INT(gaux_ptr_intcomp(GINT_TO_POINTER(3), GINT_TO_POINTER(5)), -1);
   CK_INT(gaux_ptr_intcomp(GINT_TO_POINTER(5), GINT_TO_POINTER(3)), 1);
   CK_INT(gaux_ptr_intcomp(GINT_TO_POINTER(4), GINT_TO_POINTER(4)), 0);
   CK_INT(gaux_ptr_intcomp(GINT_TO_POINTER(-2), GINT_TO_POINTER(2)), -1);
}

static void test_truncate(void) {
   const char * src[] = {"a", "b", "c", "d", "e"};

   // limit > 0: keep the first `limit` elements
   GPtrArray * a = g_ptr_array_new();
   for (int i = 0; i < 5; i++) g_ptr_array_add(a, (char *) src[i]);
   gaux_ptr_array_truncate(a, 3);
   { const char * exp[] = {"a", "b", "c"}; CK(pa_is(a, exp, 3)); }
   g_ptr_array_free(a, FALSE);

   // limit < 0: remove (len - |limit|) from the front, keeping the last |limit|
   GPtrArray * b = g_ptr_array_new();
   for (int i = 0; i < 5; i++) g_ptr_array_add(b, (char *) src[i]);
   gaux_ptr_array_truncate(b, -2);
   { const char * exp[] = {"d", "e"}; CK(pa_is(b, exp, 2)); }
   g_ptr_array_free(b, FALSE);

   // limit == 0: no change
   GPtrArray * c = g_ptr_array_new();
   for (int i = 0; i < 5; i++) g_ptr_array_add(c, (char *) src[i]);
   gaux_ptr_array_truncate(c, 0);
   CK_INT(c->len, 5);
   g_ptr_array_free(c, FALSE);

   // limit > length: no change
   GPtrArray * d = g_ptr_array_new();
   for (int i = 0; i < 5; i++) g_ptr_array_add(d, (char *) src[i]);
   gaux_ptr_array_truncate(d, 10);
   CK_INT(d->len, 5);
   g_ptr_array_free(d, FALSE);
}

static void test_append_join_copy(void) {
   const char * s1[] = {"a", "b"};
   const char * s2[] = {"c", "d", "e"};

   GPtrArray * a1 = g_ptr_array_new();
   for (int i = 0; i < 2; i++) g_ptr_array_add(a1, (char *) s1[i]);
   GPtrArray * a2 = g_ptr_array_new();
   for (int i = 0; i < 3; i++) g_ptr_array_add(a2, (char *) s2[i]);

   // append_array: no dup, shares pointers
   GPtrArray * dest = g_ptr_array_new();
   g_ptr_array_add(dest, (char *) "x");
   gaux_ptr_array_append_array(dest, a2, NULL);
   { const char * exp[] = {"x", "c", "d", "e"}; CK(pa_is(dest, exp, 4)); }
   g_ptr_array_free(dest, FALSE);

   // append_array: NULL src is a no-op
   GPtrArray * dest2 = g_ptr_array_new();
   g_ptr_array_add(dest2, (char *) "x");
   gaux_ptr_array_append_array(dest2, NULL, NULL);
   CK_INT(dest2->len, 1);
   g_ptr_array_free(dest2, FALSE);

   // join: new array = a1 ++ a2, deep-copied (owns its strings)
   GPtrArray * joined = gaux_ptr_array_join(a1, a2, dup_string, g_free);
   { const char * exp[] = {"a", "b", "c", "d", "e"}; CK(pa_is(joined, exp, 5)); }
   g_ptr_array_free(joined, TRUE);   // free func g_free set -> frees the dups

   // copy: shallow (shares pointers)
   GPtrArray * shallow = gaux_ptr_array_copy(a1, NULL, NULL);
   { const char * exp[] = {"a", "b"}; CK(pa_is(shallow, exp, 2)); }
   CK(g_ptr_array_index(shallow, 0) == g_ptr_array_index(a1, 0));   // same pointer
   g_ptr_array_free(shallow, FALSE);

   // copy: deep (own copies)
   GPtrArray * deep = gaux_ptr_array_copy(a1, dup_string, g_free);
   { const char * exp[] = {"a", "b"}; CK(pa_is(deep, exp, 2)); }
   CK(g_ptr_array_index(deep, 0) != g_ptr_array_index(a1, 0));      // distinct pointer
   g_ptr_array_free(deep, TRUE);

   g_ptr_array_free(a1, FALSE);
   g_ptr_array_free(a2, FALSE);
}

static void test_deep_copy_and_ntsa(void) {
   const char * s[] = {"one", "two", "three"};
   GPtrArray * orig = g_ptr_array_new();
   for (int i = 0; i < 3; i++) g_ptr_array_add(orig, (char *) s[i]);

   GPtrArray * dc = gaux_deep_copy_string_array(orig);
   { const char * exp[] = {"one", "two", "three"}; CK(pa_is(dc, exp, 3)); }
   CK(g_ptr_array_index(dc, 0) != g_ptr_array_index(orig, 0));   // duplicated
   g_ptr_array_free(dc, TRUE);
   g_ptr_array_free(orig, FALSE);

   // from a null-terminated array
   gpointer nt[] = {(char *) "p", (char *) "q", (char *) "r", NULL};
   GPtrArray * fromnt = gaux_ptr_array_from_null_terminated_array(nt, NULL, NULL);
   { const char * exp[] = {"p", "q", "r"}; CK(pa_is(fromnt, exp, 3)); }
   g_ptr_array_free(fromnt, FALSE);

   gpointer nt_dup[] = {(char *) "p", (char *) "q", NULL};
   GPtrArray * fromnt2 = gaux_ptr_array_from_null_terminated_array(nt_dup, dup_string, g_free);
   { const char * exp[] = {"p", "q"}; CK(pa_is(fromnt2, exp, 2)); }
   CK(g_ptr_array_index(fromnt2, 0) != nt_dup[0]);   // duplicated
   g_ptr_array_free(fromnt2, TRUE);
}

static void test_streq_and_find(void) {
   CK(gaux_streq("abc", "abc"));
   CK(!gaux_streq("abc", "abd"));
   CK(gaux_streq("", ""));

   const char * s[] = {"apple", "banana", "cherry"};
   GPtrArray * hay = g_ptr_array_new();
   for (int i = 0; i < 3; i++) g_ptr_array_add(hay, (char *) s[i]);

   guint idx = 99;
   CK(gaux_ptr_array_find_with_equal_func(hay, "banana", gaux_streq, &idx));
   CK_INT(idx, 1);

   idx = 99;
   CK(!gaux_ptr_array_find_with_equal_func(hay, "grape", gaux_streq, &idx));
   CK_INT(idx, (long) G_MAXUINT);   // set to G_MAXUINT when not found

   // NULL index_loc is allowed
   CK(gaux_ptr_array_find_with_equal_func(hay, "cherry", gaux_streq, NULL));

   // NULL equal_func -> pointer identity
   idx = 99;
   CK(gaux_ptr_array_find_with_equal_func(hay, g_ptr_array_index(hay, 2), NULL, &idx));
   CK_INT(idx, 2);

   // needle NULL, or empty haystack -> not found
   idx = 99;
   CK(!gaux_ptr_array_find_with_equal_func(hay, NULL, gaux_streq, &idx));
   CK_INT(idx, (long) G_MAXUINT);
   GPtrArray * empty = g_ptr_array_new();
   CK(!gaux_ptr_array_find_with_equal_func(empty, "x", gaux_streq, NULL));
   g_ptr_array_free(empty, FALSE);

   g_ptr_array_free(hay, FALSE);
}

static void test_thread_buffers(void) {
   // fixed buffer: allocated once, zero-filled, same pointer on later calls
   static GPrivate fkey = G_PRIVATE_INIT(g_free);
   char * f1 = get_thread_fixed_buffer(&fkey, 100);
   CK(f1 != NULL);
   bool all_zero = true;
   for (int i = 0; i < 100; i++) if (f1[i] != 0) all_zero = false;
   CK(all_zero);
   f1[0] = 'x';
   char * f2 = get_thread_fixed_buffer(&fkey, 50);   // size ignored after first alloc
   CK(f2 == f1);
   CK(f2[0] == 'x');   // same buffer, retains contents

   // dynamic buffer with a size key: grows but never shrinks
   static GPrivate dkey  = G_PRIVATE_INIT(g_free);
   static GPrivate dszkey = G_PRIVATE_INIT(g_free);
   char * d1 = get_thread_dynamic_buffer(&dkey, &dszkey, 50);
   CK(d1 != NULL);
   char * d2 = get_thread_dynamic_buffer(&dkey, &dszkey, 30);   // smaller -> no realloc
   CK(d2 == d1);
   char * d3 = get_thread_dynamic_buffer(&dkey, &dszkey, 200);  // larger -> grows
   CK(d3 != NULL);
   memset(d3, 0xff, 200);   // must be writable to the new size (ASan would catch overflow)
   CK(true);
}

int main(int argc, char ** argv) {
   test_g_list_to_g_array();
   test_comparators();
   test_truncate();
   test_append_join_copy();
   test_deep_copy_and_ntsa();
   test_streq_and_find();
   test_thread_buffers();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
