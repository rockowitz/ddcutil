/** @file test_i2c_bus_base.c
 *
 *  Standalone unit tests for the GPtrArray lookup functions in
 *  src/base/i2c_bus_base.c: finding an I2C_Bus_Info (and its index) by bus number
 *  within a caller-supplied array.  The functions that operate on the global bus
 *  table are not exercised.
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

#include "base/i2c_bus_base.h"
#include "base/i2c_bus_aux.h"    // i2c_new_bus_info / i2c_free_bus_info

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

int main(int argc, char ** argv) {
   I2C_Bus_Info * b5 = i2c_new_bus_info(5);
   I2C_Bus_Info * b6 = i2c_new_bus_info(6);

   GPtrArray * buses = g_ptr_array_new();
   g_ptr_array_add(buses, b5);
   g_ptr_array_add(buses, b6);

   // find the record by bus number
   CK(i2c_find_bus_info_in_gptrarray_by_busno(buses, 5) == b5);
   CK(i2c_find_bus_info_in_gptrarray_by_busno(buses, 6) == b6);
   CK(i2c_find_bus_info_in_gptrarray_by_busno(buses, 99) == NULL);

   // find the index by bus number
   CK_INT(i2c_find_bus_info_index_in_gptrarray_by_busno(buses, 5), 0);
   CK_INT(i2c_find_bus_info_index_in_gptrarray_by_busno(buses, 6), 1);
   CK_INT(i2c_find_bus_info_index_in_gptrarray_by_busno(buses, 99), -1);
   CK_INT(i2c_find_bus_info_index_in_gptrarray_by_busno(NULL, 5), -1);

   g_ptr_array_free(buses, FALSE);
   i2c_free_bus_info(b5);
   i2c_free_bus_info(b6);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
