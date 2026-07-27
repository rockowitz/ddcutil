/** @file test_string_util.c
 *
 *  Standalone unit tests for the functions in src/util/string_util.c.
 *
 *  Checks that each function produces the expected output for normal inputs,
 *  edge cases (empty strings, boundaries), and, where the function is defined
 *  to handle them, NULL inputs.  Prints one line per failing check and a
 *  summary; exit status is 0 if all checks pass, 1 otherwise.
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

#include "util/coredefs_base.h"   // Byte
#include "util/string_util.h"

static int total = 0;
static int failed = 0;

static bool streq0(const char * a, const char * b) {
   if (a == NULL && b == NULL) return true;
   if (a == NULL || b == NULL) return false;
   return strcmp(a, b) == 0;
}

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

// integer-valued check with reporting
#define CK_INT(expr, expected) do { \
   total++; \
   long _a = (long)(expr); long _e = (long)(expected); \
   if (_a != _e) { failed++; \
      printf("FAIL  line %-4d  %s -> %ld, expected %ld\n", __LINE__, #expr, _a, _e); } \
} while(0)

// string check against a non-owned (or literal) string; does NOT free
#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (!streq0(_a, _e)) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e ? _e : "(null)"); } \
} while(0)

// string check for a function returning newly allocated memory; frees it
#define CK_STR_FREE(expr, expected) do { \
   total++; \
   char * _a = (expr); const char * _e = (expected); \
   if (!streq0(_a, _e)) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #expr, \
             _a ? _a : "(null)", _e ? _e : "(null)"); } \
   free(_a); \
} while(0)

static void test_compare(void) {
   CK(streq("abc", "abc"));
   CK(!streq("abc", "abd"));
   CK(streq(NULL, NULL));
   CK(!streq("abc", NULL));
   CK(!streq(NULL, "abc"));
   CK(streq("", ""));

   CK(streqcase("aBc", "AbC"));
   CK(!streqcase("abc", "abd"));
   CK(streqcase(NULL, NULL));
   CK(!streqcase("abc", NULL));

   // is_abbrev(value, longname, minchars)
   CK(is_abbrev("ab", "abcdef", 2));
   CK(is_abbrev("abcdef", "abcdef", 2));
   CK(!is_abbrev("ab", "abcdef", 3));       // shorter than minchars
   CK(!is_abbrev("abx", "abcdef", 2));      // not a prefix
   CK(!is_abbrev("abcdefg", "abcdef", 2));  // longer than longname
   CK(is_abbrev("", "abc", 0));             // empty value, minchars 0
   CK(!is_abbrev(NULL, "abc", 1));
   CK(!is_abbrev("abc", NULL, 1));

   CK(str_starts_with("abcdef", "abc"));
   CK(!str_starts_with("abcdef", "xyz"));
   CK(!str_starts_with("abc", "abcdef"));   // prefix longer than value
   CK(str_starts_with("abc", ""));          // empty prefix matches
   CK(!str_starts_with(NULL, "abc"));
   CK(!str_starts_with("abc", NULL));

   CK(str_ends_with("abcdef", "def"));
   CK(!str_ends_with("abcdef", "abc"));
   CK(!str_ends_with("abc", "abcdef"));     // suffix longer than value
   CK(str_ends_with("abc", ""));            // empty suffix matches
   CK(str_ends_with("abc", "abc"));

   CK_INT(str_contains("hello world", "world"), 6);
   CK_INT(str_contains("hello world", "hello"), 0);
   CK_INT(str_contains("hello", "xyz"), -1);
   CK_INT(str_contains("hello", ""), 0);      // empty segment found at 0
   CK_INT(str_contains("abc", "abcd"), -1);   // segment longer than value
   CK_INT(str_contains(NULL, "x"), -1);
   CK_INT(str_contains("x", NULL), -1);

   CK(str_all_printable("Hello, World!"));
   CK(!str_all_printable("ab\tc"));           // tab not printable
   CK(str_all_printable(""));
   CK(str_all_printable(NULL));               // NULL -> true per contract
}

static void test_case(void) {
   char buf[8];

   strcpy(buf, "aBc123"); strupper(buf); CK_STR(buf, "ABC123");
   strcpy(buf, "aBc123"); strlower(buf); CK_STR(buf, "abc123");
   strupper(NULL);   // must not crash
   strlower(NULL);

   CK_STR_FREE(strdup_uc("aBc"), "ABC");
   CK(strdup_uc(NULL) == NULL);
}

static void test_trim(void) {
   CK_STR_FREE(strtrim("  abc  "), "abc");
   CK_STR_FREE(strtrim("abc"), "abc");
   CK_STR_FREE(strtrim("   "), "");            // all whitespace
   CK_STR_FREE(strtrim(""), "");
   CK_STR_FREE(strtrim("\t abc\n"), "abc");

   char rbuf[4];
   CK_STR(strtrim_r("  abcdef  ", rbuf, sizeof(rbuf)), "abc");   // truncated to bufsz-1
   char rbuf2[16];
   CK_STR(strtrim_r("  hi  ", rbuf2, sizeof(rbuf2)), "hi");

   char b1[16]; strcpy(b1, "   abc");   CK_STR(ltrim_in_place(b1), "abc");
   char b2[16]; strcpy(b2, "abc   ");   CK_STR(rtrim_in_place(b2), "abc");
   char b3[16]; strcpy(b3, "  abc  ");  CK_STR(trim_in_place(b3), "abc");
   char b4[16]; strcpy(b4, "abc\n");    CK_STR(rtrim_in_place(b4), "abc");
}

static void test_substr(void) {
   CK_STR_FREE(substr("hello", 1, 3), "ell");
   CK_STR_FREE(substr("hello", 1, 100), "ello");   // ct clamped to length
   CK_STR_FREE(substr("hello", 10, 3), "");        // startpos past end
   CK_STR_FREE(substr("hello", 0, 0), "");
   CK_STR_FREE(substr("hello", 0, 5), "hello");

   CK_STR_FREE(lsub("hello", 3), "hel");
   CK_STR_FREE(lsub("hello", 100), "hello");
   CK_STR_FREE(lsub("hello", 0), "");
}

static void test_build(void) {
   const char * pieces[] = {"a", "b", "c", NULL};
   CK_STR_FREE(strjoin(pieces, 3, ","), "a,b,c");
   CK_STR_FREE(strjoin(pieces, -1, ","), "a,b,c");   // -1 -> null terminated
   CK_STR_FREE(strjoin(pieces, 3, NULL), "abc");     // NULL separator
   CK_STR_FREE(strjoin(pieces, 1, ","), "a");
   const char * one[] = {"only", NULL};
   CK_STR_FREE(strjoin(one, -1, ", "), "only");

   CK_STR_FREE(strcat_new("abc", "def"), "abcdef");
   CK_STR_FREE(strcat_new("", "def"), "def");
   CK_STR_FREE(strcat_new("abc", ""), "abc");

   CK_STR_FREE(chars_to_string("hello world", 5), "hello");
   CK_STR_FREE(chars_to_string("hello", 0), "");
   CK(chars_to_string(NULL, 5) == NULL);

   char rc[16]; strcpy(rc, "a-b-c-d");
   CK_STR(str_replace_char(rc, '-', '_'), "a_b_c_d");
   char rc2[16]; strcpy(rc2, "abc");
   CK_STR(str_replace_char(rc2, 'z', '_'), "abc");   // no match, unchanged

   CK_STR_FREE(int_array_to_string((uint16_t[]){1, 22, 333}, 3), "1, 22, 333");
}

static void test_sbuf_append(void) {
   char buf[20];
   buf[0] = '\0';
   CK(!sbuf_append(buf, sizeof(buf), ",", "abc"));   // first append, no sep
   CK_STR(buf, "abc");
   CK(!sbuf_append(buf, sizeof(buf), ",", "def"));
   CK_STR(buf, "abc,def");

   char small[8];
   small[0] = '\0';
   CK(!sbuf_append(small, sizeof(small), NULL, "abcdef"));   // fits (6 <= 7)
   CK_STR(small, "abcdef");
   CK(sbuf_append(small, sizeof(small), ",", "xyz"));        // overflow -> truncated
}

static void test_match(void) {
   const char * list[] = {"apple", "banana", "cherry", NULL};
   CK_INT(exactly_matches_any("banana", list), 1);
   CK_INT(exactly_matches_any("apple", list), 0);
   CK_INT(exactly_matches_any("grape", list), -1);
   CK_INT(exactly_matches_any("BANANA", list), -1);   // case sensitive

   CK_INT(exactly_matches_any_case("banana", list), 1);
   CK_INT(exactly_matches_any_case("BANANA", list), 1);   // case insensitive
   CK_INT(exactly_matches_any_case("Apple", list), 0);
   CK_INT(exactly_matches_any_case("grape", list), -1);
   CK_INT(exactly_matches_any_case("apples", list), -1);  // not exact (extra char)
   CK_INT(exactly_matches_any_case("app", list), -1);     // not exact (prefix only)

   const char * prefixes[] = {"xy", "ab", "qr", NULL};
   CK_INT(starts_with_any("abcdef", prefixes), 1);
   CK_INT(starts_with_any("zzz", prefixes), -1);

   CK_INT(exactly_matches_anyv("banana", "apple", "banana", "cherry", NULL), 1);
   CK_INT(exactly_matches_anyv("apple",  "apple", "banana", "cherry", NULL), 0);
   CK_INT(exactly_matches_anyv("grape",  "apple", "banana", "cherry", NULL), -1);
   CK_INT(exactly_matches_anyv("BANANA", "apple", "banana", "cherry", NULL), -1);  // case sensitive
   CK_INT(exactly_matches_anyv("apples", "apple", "banana", "cherry", NULL), -1);  // not exact
   CK_INT(exactly_matches_anyv("only",   "only", NULL), 0);       // single candidate
   CK_INT(exactly_matches_anyv("nope",   NULL), -1);              // no candidates

   // EXACTLY_MATCHES_ANYV() is a macro: no terminating NULL is supplied,
   // it is appended automatically.
   CK_INT(EXACTLY_MATCHES_ANYV("banana", "apple", "banana", "cherry"), 1);
   CK_INT(EXACTLY_MATCHES_ANYV("apple",  "apple", "banana", "cherry"), 0);
   CK_INT(EXACTLY_MATCHES_ANYV("grape",  "apple", "banana", "cherry"), -1);
   CK_INT(EXACTLY_MATCHES_ANYV("BANANA", "apple", "banana", "cherry"), -1);  // case sensitive
   CK_INT(EXACTLY_MATCHES_ANYV("apples", "apple", "banana", "cherry"), -1);  // not exact
   CK_INT(EXACTLY_MATCHES_ANYV("only",   "only"), 0);       // single candidate
   CK_INT(EXACTLY_MATCHES_ANYV("nope"), -1);                // no candidates
}

static void test_numeric(void) {
   long lval = 0;
   CK(str_to_long("12345", &lval, 10) && lval == 12345);
   CK(str_to_long("-42", &lval, 10) && lval == -42);
   CK(str_to_long("ff", &lval, 16) && lval == 255);
   CK(str_to_long("0xff", &lval, 16) && lval == 255);
   CK(str_to_long("xff", &lval, 16) && lval == 255);      // x prefix
   CK(str_to_long("10", &lval, 0) && lval == 10);         // base 0: decimal
   CK(str_to_long("0x10", &lval, 0) && lval == 16);       // base 0: hex
   CK(!str_to_long("abc", &lval, 10));
   CK(!str_to_long("12x", &lval, 10));                    // trailing junk
   CK(!str_to_long("", &lval, 10));
   CK(!str_to_long(NULL, &lval, 10));
   CK(!str_to_long("x", &lval, 16));                      // no digits after x

   int ival = 0;
   CK(str_to_int("123", &ival, 10) && ival == 123);
   CK(str_to_int("0x1F", &ival, 16) && ival == 31);
   CK(!str_to_int("abc", &ival, 10));
   CK(!str_to_int("9999999999", &ival, 10));              // does not fit in int

   float fval = 0;
   CK(str_to_float("3.5", &fval) && fval == 3.5f);
   CK(str_to_float("-0.25", &fval) && fval == -0.25f);
   CK(str_to_float("10", &fval) && fval == 10.0f);
   CK(!str_to_float("abc", &fval));
   CK(!str_to_float("3.5x", &fval));
   CK(!str_to_float("", &fval));
}

static void test_hex_conv(void) {
   Byte b = 0;
   CK(hhs_to_byte_in_buf("ff", &b) && b == 0xff);
   CK(hhs_to_byte_in_buf("1a", &b) && b == 0x1a);
   CK(hhs_to_byte_in_buf("00", &b) && b == 0x00);
   CK(!hhs_to_byte_in_buf("f", &b));       // wrong length
   CK(!hhs_to_byte_in_buf("fff", &b));
   CK(!hhs_to_byte_in_buf("gg", &b));      // not hex

   CK(hhc_to_byte_in_buf("ffxx", &b) && b == 0xff);   // only first 2 chars used
   CK(hhc_to_byte_in_buf("1a", &b) && b == 0x1a);

   CK(any_one_byte_hex_string_to_byte_in_buf("0xff", &b) && b == 0xff);
   CK(any_one_byte_hex_string_to_byte_in_buf("ff", &b) && b == 0xff);
   CK(any_one_byte_hex_string_to_byte_in_buf("f", &b) && b == 0x0f);   // single digit
   CK(any_one_byte_hex_string_to_byte_in_buf("1aH", &b) && b == 0x1a); // h suffix
   CK(any_one_byte_hex_string_to_byte_in_buf("x1a", &b) && b == 0x1a); // x prefix
   CK(!any_one_byte_hex_string_to_byte_in_buf("zz", &b));

   Byte * ba = NULL;
   CK_INT(hhs_to_byte_array("0a0b0c", &ba), 3);
   if (ba) { CK(ba[0] == 0x0a && ba[1] == 0x0b && ba[2] == 0x0c); free(ba); ba = NULL; }
   CK_INT(hhs_to_byte_array("", &ba), 0);   // empty -> 0 bytes
   if (ba) { free(ba); ba = NULL; }
   CK_INT(hhs_to_byte_array("0a0", &ba), -1);  // odd length
   CK_INT(hhs_to_byte_array("zz", &ba), -1);   // not hex

   uint16_t u16 = 0;
   CK(hhs4_to_uint16("1a2b", &u16) && u16 == 0x1a2b);
   CK(hhs4_to_uint16("ffff", &u16) && u16 == 0xffff);
   CK(!hhs4_to_uint16("1a2", &u16));       // wrong length
   CK(!hhs4_to_uint16("zzzz", &u16));      // not hex

   CK_STR_FREE(canonicalize_possible_hex_value("x1a"), "0x1a");
   CK_STR_FREE(canonicalize_possible_hex_value("1aH"), "0x1a");
   CK_STR_FREE(canonicalize_possible_hex_value("1ah"), "0x1a");
   CK_STR_FREE(canonicalize_possible_hex_value("0X1a"), "0x1a");
   CK_STR_FREE(canonicalize_possible_hex_value("123"), "123");   // no hex marker
}

static void test_hexstring(void) {
   Byte bytes[] = {0x01, 0x0a, 0xff};
   CK_STR_FREE(hexstring(bytes, 3), "01 0a ff");
   CK_STR_FREE(hexstring(bytes, 0), "");
   CK_STR_FREE(hexstring(bytes, 1), "01");

   CK_STR_FREE(hexstring2(bytes, 3, ":", true, NULL, 0), "01:0A:FF");
   CK_STR_FREE(hexstring2(bytes, 3, ":", false, NULL, 0), "01:0a:ff");
   CK_STR_FREE(hexstring2(bytes, 3, NULL, false, NULL, 0), "010aff");
   CK_STR_FREE(hexstring2(bytes, 0, ":", false, NULL, 0), "");

   char hbuf[32];
   CK_STR(hexstring2(bytes, 3, "-", true, hbuf, sizeof(hbuf)), "01-0A-FF");  // caller buffer
}

static void test_misc(void) {
   Byte z[] = {0, 0, 0};
   Byte nz[] = {0, 1, 0};
   CK(all_bytes_zero(z, 3));
   CK(!all_bytes_zero(nz, 3));
   CK(all_bytes_zero(z, 0));   // vacuously true

   CK_STR(ascii_strcasestr("Hello World", "world"), "World");   // returns tail of haystack
   CK_STR(ascii_strcasestr("Hello World", "HELLO"), "Hello World");
   CK(ascii_strcasestr("abc", "xyz") == NULL);
   CK(ascii_strcasestr(NULL, "x") == NULL);

   const char * s1 = "abc";
   const char * s2 = "abd";
   const char * s3 = "abc";
   CK(indirect_strcmp(&s1, &s2) < 0);
   CK(indirect_strcmp(&s2, &s1) > 0);
   CK(indirect_strcmp(&s1, &s3) == 0);
}

static void test_ntsa(void) {
   // strsplit, skipping empty tokens
   Null_Terminated_String_Array a = strsplit("a,b,c", ",");
   CK_INT(ntsa_length(a), 3);
   CK_STR(a[0], "a"); CK_STR(a[1], "b"); CK_STR(a[2], "c"); CK(a[3] == NULL);
   CK_INT(ntsa_find(a, "b"), 1);
   CK_INT(ntsa_find(a, "z"), -1);
   CK_INT(ntsa_findx(a, "C", streqcase), 2);

   Null_Terminated_String_Array empties = strsplit("a,,b", ",");
   CK_INT(ntsa_length(empties), 2);    // empty token skipped
   CK_STR(empties[0], "a"); CK_STR(empties[1], "b");
   ntsa_free(empties, true);

   Null_Terminated_String_Array none = strsplit("", ",");
   CK_INT(ntsa_length(none), 0);
   ntsa_free(none, true);

   Null_Terminated_String_Array nullsplit = strsplit(NULL, ",");
   CK_INT(ntsa_length(nullsplit), 0);
   ntsa_free(nullsplit, true);

   // ntsa_copy
   Null_Terminated_String_Array cp = ntsa_copy(a, true);
   CK_INT(ntsa_length(cp), 3);
   CK_STR(cp[1], "b");
   ntsa_free(cp, true);

   // ntsa_prepend
   Null_Terminated_String_Array pre = ntsa_prepend("z", a, true);
   CK_INT(ntsa_length(pre), 4);
   CK_STR(pre[0], "z"); CK_STR(pre[1], "a");
   ntsa_free(pre, true);

   // ntsa_join
   Null_Terminated_String_Array b = strsplit("d,e", ",");
   Null_Terminated_String_Array joined = ntsa_join(a, b, true);
   CK_INT(ntsa_length(joined), 5);
   CK_STR(joined[0], "a"); CK_STR(joined[3], "d"); CK_STR(joined[4], "e");
   ntsa_free(joined, true);
   ntsa_free(b, true);

   // strsplit_maxlength: no delims -> fixed-size chunks
   Null_Terminated_String_Array chunks = strsplit_maxlength("abcdefg", 3, NULL);
   CK_INT(ntsa_length(chunks), 3);
   CK_STR(chunks[0], "abc"); CK_STR(chunks[1], "def"); CK_STR(chunks[2], "g");
   ntsa_free(chunks, true);

   // strsplit_maxlength with delims -> break at delimiter within max length
   Null_Terminated_String_Array words = strsplit_maxlength("aa bb cc", 5, " ");
   CK_STR(words[0], "aa ");    // breaks after the space within 5 chars
   ntsa_free(words, true);

   ntsa_free(a, true);

   // empty array
   Null_Terminated_String_Array empty = ntsa_create_empty_array();
   CK_INT(ntsa_length(empty), 0);
   ntsa_free(empty, true);
}

int main(int argc, char ** argv) {
   test_compare();
   test_case();
   test_trim();
   test_substr();
   test_build();
   test_sbuf_append();
   test_match();
   test_numeric();
   test_hex_conv();
   test_hexstring();
   test_misc();
   test_ntsa();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
