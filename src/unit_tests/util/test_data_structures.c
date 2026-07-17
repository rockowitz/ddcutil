/** @file test_data_structures.c
 *
 *  Standalone unit tests for the abstract data types in
 *  src/util/data_structures.c: Byte_Value_Array (bva_*), Bit_Set_256 (bs256_*),
 *  Value_Name_Title lookups (vnt_*), Buffer (buffer_*), and
 *  Circular_String_Buffer (csb_*).
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/coredefs_base.h"    // Byte
#include "util/data_structures.h"

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
   if (strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, _a, _e); } \
} while(0)

static bool is_even(int i) { return (i % 2) == 0; }

static void test_bva(void) {
   Byte_Value_Array bva = bva_create();
   CK(bva != NULL);
   CK_INT(bva_length(bva), 0);

   bva_append(bva, 10);
   bva_append(bva, 30);
   bva_append(bva, 20);
   CK_INT(bva_length(bva), 3);
   CK_INT(bva_get(bva, 0), 10);
   CK_INT(bva_get(bva, 1), 30);
   CK_INT(bva_get(bva, 2), 20);
   CK(bva_contains(bva, 30));
   CK(!bva_contains(bva, 99));

   // as_string preserves insertion order
   CK_STR(bva_as_string(bva, false, ","), "10,30,20");
   CK_STR(bva_as_string(bva, true, " "),  "0a 1e 14");
   CK_STR(bva_as_string(bva, true, ""),   "0a1e14");

   bva_sort(bva);
   CK_INT(bva_get(bva, 0), 10);
   CK_INT(bva_get(bva, 1), 20);
   CK_INT(bva_get(bva, 2), 30);

   // sorted_eq compares element-wise; it assumes the inputs are already sorted
   Byte_Value_Array bvb = bva_create();
   bva_append(bvb, 30); bva_append(bvb, 20); bva_append(bvb, 10);
   bva_sort(bvb);
   CK(bva_sorted_eq(bva, bvb));       // both now {10,20,30}
   bva_append(bvb, 40);
   CK(!bva_sorted_eq(bva, bvb));      // different length
   bva_free(bvb);

   // filter
   Byte_Value_Array bvc = bva_create();
   for (int i = 1; i <= 4; i++) bva_append(bvc, i);
   Byte_Value_Array evens = bva_filter(bvc, is_even);
   CK_INT(bva_length(evens), 2);
   CK_STR(bva_as_string(evens, false, ","), "2,4");
   bva_free(evens);
   bva_free(bvc);

   // empty as_string
   Byte_Value_Array empty = bva_create();
   CK_STR(bva_as_string(empty, false, ","), "");
   bva_free(empty);

   bva_free(bva);
}

static void test_bs256(void) {
   Bit_Set_256 s = EMPTY_BIT_SET_256;
   CK_INT(bs256_count(s), 0);
   CK_INT(bs256_first_bit_set(s), -1);
   CK(!bs256_contains(s, 5));

   s = bs256_insert(s, 5);
   s = bs256_insert(s, 200);
   s = bs256_insert(s, 5);      // duplicate insert is a no-op
   CK_INT(bs256_count(s), 2);
   CK(bs256_contains(s, 5));
   CK(bs256_contains(s, 200));
   CK(!bs256_contains(s, 6));
   CK_INT(bs256_first_bit_set(s), 5);

   Bit_Set_256 s2 = bs256_remove(s, 5);
   CK(!bs256_contains(s2, 5));
   CK(bs256_contains(s2, 200));
   CK_INT(bs256_count(s2), 1);
   CK(bs256_contains(s, 5));     // original unchanged (value semantics)

   // equality
   Bit_Set_256 a = bs256_insert(bs256_insert(EMPTY_BIT_SET_256, 1), 2);
   Bit_Set_256 b = bs256_insert(bs256_insert(EMPTY_BIT_SET_256, 2), 1);
   CK(bs256_eq(a, b));
   CK(!bs256_eq(a, s));

   // set algebra
   Bit_Set_256 x = bs256_insert(bs256_insert(EMPTY_BIT_SET_256, 1), 2);
   Bit_Set_256 y = bs256_insert(bs256_insert(EMPTY_BIT_SET_256, 2), 3);
   CK_INT(bs256_count(bs256_or(x, y)), 3);       // {1,2,3}
   CK_INT(bs256_count(bs256_and(x, y)), 1);      // {2}
   CK(bs256_contains(bs256_and(x, y), 2));
   CK_INT(bs256_count(bs256_and_not(x, y)), 1);  // {1}
   CK(bs256_contains(bs256_and_not(x, y), 1));

   // subset
   Bit_Set_256 sub = bs256_insert(EMPTY_BIT_SET_256, 2);
   CK(bs256_is_subset(sub, x));
   CK(!bs256_is_subset(x, sub));
   CK(bs256_is_subset(EMPTY_BIT_SET_256, x));   // empty is a subset of anything

   // string representations (bits emitted in ascending order)
   Bit_Set_256 t = bs256_insert(bs256_insert(bs256_insert(EMPTY_BIT_SET_256, 1), 5), 10);
   CK_STR(bs256_to_string_decimal_t(t, "", ","), "1,5,10");
   CK_STR(bs256_to_string_t(t, "", " "), "01 05 0a");
   CK_STR(bs256_to_string_decimal_t(t, "x", ","), "x1,x5,x10");
   CK_STR(bs256_to_string_decimal_t(EMPTY_BIT_SET_256, "", ","), "");

   // to_bytes
   Byte buf[8];
   int n = bs256_to_bytes(t, buf, sizeof(buf));
   CK_INT(n, 3);
   CK(buf[0] == 1 && buf[1] == 5 && buf[2] == 10);

   // iterator yields set bits in ascending order, then -1
   Bit_Set_256_Iterator iter = bs256_iter_new(t);
   CK_INT(bs256_iter_next(iter), 1);
   CK_INT(bs256_iter_next(iter), 5);
   CK_INT(bs256_iter_next(iter), 10);
   CK_INT(bs256_iter_next(iter), -1);
   bs256_iter_reset(iter);
   CK_INT(bs256_iter_next(iter), 1);   // reset restarts
   bs256_iter_free(iter);

   // conversion to/from Byte_Value_Array
   Byte_Value_Array bva = bva_create();
   bva_append(bva, 1); bva_append(bva, 5); bva_append(bva, 10);
   Bit_Set_256 frombva = bs256_from_bva(bva);
   CK(bs256_eq(frombva, t));
   CK(bva_bs256_same_values(bva, t));
   bva_append(bva, 200);
   CK(!bva_bs256_same_values(bva, t));
   bva_free(bva);
}

static Value_Name_Title test_table[] = {
   {1, "ONE",   "The number one"},
   {2, "TWO",   "The number two"},
   {3, "THREE", "The number three"},
   VNT_END
};

static void test_vnt(void) {
   CK_STR(vnt_name(test_table, 2), "TWO");
   CK_STR(vnt_title(test_table, 3), "The number three");
   CK(vnt_name(test_table, 99) == NULL);
   CK(vnt_title(test_table, 99) == NULL);

   // find_id by symbolic name
   CK_INT(vnt_find_id(test_table, "TWO", false, false, -1), 2);
   CK_INT(vnt_find_id(test_table, "two", false, true,  -1), 2);   // ignore case
   CK_INT(vnt_find_id(test_table, "two", false, false, -1), -1);  // case sensitive miss
   // find_id by title
   CK_INT(vnt_find_id(test_table, "The number three", true, false, -1), 3);
   // default when not found
   CK_INT(vnt_find_id(test_table, "nope", false, false, -99), -99);
}

static void test_buffer(void) {
   Buffer * b = buffer_new(16, "test");
   CK(b != NULL);
   CK_INT(buffer_length(b), 0);

   buffer_add(b, 0x01);
   buffer_add(b, 0x02);
   CK_INT(buffer_length(b), 2);
   CK(b->bytes[0] == 0x01 && b->bytes[1] == 0x02);

   Byte more[] = {0x03, 0x04};
   buffer_append(b, more, 2);
   CK_INT(buffer_length(b), 4);
   CK(b->bytes[3] == 0x04);

   // buffer_put replaces content
   Byte repl[] = {0xaa, 0xbb};
   buffer_put(b, repl, 2);
   CK_INT(buffer_length(b), 2);
   CK(b->bytes[0] == 0xaa && b->bytes[1] == 0xbb);

   buffer_set_byte(b, 0, 0xcc);
   CK(b->bytes[0] == 0xcc);

   // new_with_value and equality
   Byte v[]  = {1, 2, 3};
   Byte v2[] = {1, 2, 4};
   Buffer * b1 = buffer_new_with_value(v, 3, "b1");
   Buffer * b2 = buffer_new_with_value(v, 3, "b2");
   Buffer * b3 = buffer_new_with_value(v2, 3, "b3");
   CK_INT(buffer_length(b1), 3);
   CK(buffer_eq(b1, b2));       // same contents
   CK(!buffer_eq(b1, b3));      // same length, different contents
   buffer_free(b1, "b1");
   buffer_free(b2, "b2");
   buffer_free(b3, "b3");
   buffer_free(b, "test");
}

static void test_csb(void) {
   // fewer additions than capacity: all retained, in order
   Circular_String_Buffer * csb = csb_new(3);
   csb_add(csb, "a", true);
   csb_add(csb, "b", true);
   GPtrArray * pa = csb_to_g_ptr_array(csb);
   CK_INT(pa->len, 2);
   CK_STR((char *) g_ptr_array_index(pa, 0), "a");
   CK_STR((char *) g_ptr_array_index(pa, 1), "b");
   g_ptr_array_free(pa, FALSE);
   csb_free(csb, true);

   // more additions than capacity: only the last `size`, in chronological order
   Circular_String_Buffer * csb2 = csb_new(3);
   csb_add(csb2, "a", true);
   csb_add(csb2, "b", true);
   csb_add(csb2, "c", true);
   csb_add(csb2, "d", true);
   csb_add(csb2, "e", true);
   GPtrArray * pa2 = csb_to_g_ptr_array(csb2);
   CK_INT(pa2->len, 3);
   CK_STR((char *) g_ptr_array_index(pa2, 0), "c");
   CK_STR((char *) g_ptr_array_index(pa2, 1), "d");
   CK_STR((char *) g_ptr_array_index(pa2, 2), "e");
   g_ptr_array_free(pa2, FALSE);
   csb_free(csb2, true);
}

int main(int argc, char ** argv) {
   test_bva();
   test_bs256();
   test_vnt();
   test_buffer();
   test_csb();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
