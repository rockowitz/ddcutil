/** @file test_i2c_util.c
 *
 *  Standalone unit tests for the pure functions in src/util/i2c_util.c:
 *  parsing a bus number out of an "i2c-N" name, extracting the number after a
 *  hyphen, the qsort-style bus-name comparator, and the functionality-flag
 *  interpreter.  The functions that query a real /dev/i2c file descriptor are
 *  not exercised.
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
#include <linux/i2c.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/i2c_util.h"

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

static void test_name_to_busno(void) {
   CK_INT(i2c_name_to_busno("i2c-5"), 5);
   CK_INT(i2c_name_to_busno("i2c-0"), 0);
   CK_INT(i2c_name_to_busno("i2c-15"), 15);
   CK_INT(i2c_name_to_busno("i2c-"), -1);       // no digits
   CK_INT(i2c_name_to_busno("i2c--1"), -1);      // hyphen where a digit is expected
   CK_INT(i2c_name_to_busno("foo-3"), -1);       // wrong prefix
   CK_INT(i2c_name_to_busno(NULL), -1);
}

static void test_extract_number(void) {
   CK_INT(extract_number_after_hyphen("bus-9"), 9);
   CK_INT(extract_number_after_hyphen("i2c-42"), 42);
   CK_INT(extract_number_after_hyphen("plain"), -1);       // no hyphen
   CK_INT(extract_number_after_hyphen("trailing-"), -1);   // nothing after
   CK_INT(extract_number_after_hyphen("a-b"), -1);         // not a number
   CK_INT(extract_number_after_hyphen(NULL), -1);
}

static void test_compare(void) {
   char * a = "i2c-2";
   char * b = "i2c-10";
   char * c = "i2c-2";
   CK(i2c_compare(&a, &b) < 0);      // 2 before 10 (numeric, not lexical)
   CK(i2c_compare(&b, &a) > 0);
   CK_INT(i2c_compare(&a, &c), 0);   // equal bus numbers
   // NULL element handling
   CK(i2c_compare(NULL, &b) < 0);
   CK(i2c_compare(&a, NULL) > 0);
   CK_INT(i2c_compare(NULL, NULL), 0);
}

static void test_functionality_flags(void) {
   // no bits set -> empty string
   char * s = i2c_interpret_functionality_flags(0);
   CK_STR(s, "");
   free(s);

   // single flag -> its symbolic name
   s = i2c_interpret_functionality_flags(I2C_FUNC_I2C);
   CK_STR(s, "I2C_FUNC_I2C");
   free(s);

   // multiple flags -> comma-separated, in table order
   s = i2c_interpret_functionality_flags(I2C_FUNC_I2C | I2C_FUNC_SMBUS_PEC);
   CK_STR(s, "I2C_FUNC_I2C, I2C_FUNC_SMBUS_PEC");
   free(s);
}

int main(int argc, char ** argv) {
   test_name_to_busno();
   test_extract_number();
   test_compare();
   test_functionality_flags();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
