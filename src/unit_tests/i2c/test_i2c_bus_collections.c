/** @file test_i2c_bus_collections.c
 *
 *  Standalone unit tests for the host-independent functions of
 *  src/i2c/i2c_bus_collections.c: constructing a Bit_Set_256 of bus numbers
 *  from a caller-supplied array of I2C_Bus_Info, and the
 *  force_failure_i2c_all_relevant_i2c_buses_rw test hook of
 *  i2c_all_relevant_i2c_buses_rw().  The bus-detection and EDID-probing
 *  functions, which require real /dev/i2c devices, are not exercised.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libi2c unit test: it links the internal libi2c/libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/error_info.h"

#include "base/i2c_bus_aux.h"
#include "base/i2c_bus_base.h"

#include "i2c/i2c_bus_collections.h"

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


// Sentinel value stored in I2C_Bus_Info.edid to simulate "EDID present"
// without allocating a real Parsed_Edid.  These entries are never freed
// with i2c_free_bus_info() (which would try to free_parsed_edid() the
// bogus pointer); the businfo structs themselves are freed directly.
#define FAKE_EDID ((Parsed_Edid *) 1)

static void test_bitset_from_businfo_array(void) {
   I2C_Bus_Info * a = i2c_new_bus_info(3);   // no edid, not laptop
   I2C_Bus_Info * b = i2c_new_bus_info(5);   // edid, not laptop
   I2C_Bus_Info * c = i2c_new_bus_info(7);   // edid, laptop

   b->edid = FAKE_EDID;
   c->edid = FAKE_EDID;
   c->flags |= I2C_BUS_APPARENT_LAPTOP;

   GPtrArray * buses = g_ptr_array_new();
   g_ptr_array_add(buses, a);
   g_ptr_array_add(buses, b);
   g_ptr_array_add(buses, c);

   Bit_Set_256 all3     = bs256_insert(bs256_insert(bs256_insert(EMPTY_BIT_SET_256, 3), 5), 7);
   Bit_Set_256 connected = bs256_insert(bs256_insert(EMPTY_BIT_SET_256, 5), 7);
   Bit_Set_256 nonlaptop_all  = bs256_insert(bs256_insert(EMPTY_BIT_SET_256, 3), 5);
   Bit_Set_256 nonlaptop_conn = bs256_insert(EMPTY_BIT_SET_256, 5);

   CK(bs256_eq(i2c_buses_bitset_from_businfo_array(buses, false), all3));
   CK(bs256_eq(i2c_buses_bitset_from_businfo_array(buses, true),  connected));
   CK(bs256_eq(i2c_nonlaptop_buses_bitset_from_businfo_array(buses, false), nonlaptop_all));
   CK(bs256_eq(i2c_nonlaptop_buses_bitset_from_businfo_array(buses, true),  nonlaptop_conn));

   CK_INT(bs256_count(all3), 3);
   CK_INT(bs256_count(connected), 2);

   g_ptr_array_free(buses, true);
   a->edid = NULL;   // already NULL, but explicit for symmetry
   b->edid = NULL;   // avoid i2c_free_bus_info() trying to free the sentinel
   c->edid = NULL;
   i2c_free_bus_info(a);
   i2c_free_bus_info(b);
   i2c_free_bus_info(c);
}


static void test_force_failure_hook(void) {
   CK(force_failure_i2c_all_relevant_i2c_buses_rw == false);   // default

   force_failure_i2c_all_relevant_i2c_buses_rw = true;
   Error_Info * err = i2c_all_relevant_i2c_buses_rw();
   CK(err != NULL);
   if (err) {
      CK_INT(err->status_code, -EACCES);
      errinfo_free(err);
   }
   force_failure_i2c_all_relevant_i2c_buses_rw = false;   // restore
}


int main(int argc, char ** argv) {
   test_bitset_from_businfo_array();
   test_force_failure_hook();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
