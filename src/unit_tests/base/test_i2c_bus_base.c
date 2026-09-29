/** @file test_i2c_bus_base.c
 *
 *  Standalone unit tests for the GPtrArray lookup functions in
 *  src/base/i2c_bus_base.c: finding an I2C_Bus_Info (and its index) by bus number
 *  within a caller-supplied array, and the lifecycle of the global bus table:
 *  the arrays are created by init_i2c_bus_base() and exist for the life of the
 *  process, i2c_discard_buses() empties all_i2c_buses rather than destroying it,
 *  and a removed record is moved to removed_i2c_buses rather than freed.
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
#include <unistd.h>

#include "base/i2c_bus_base.h"
#include "base/i2c_bus_aux.h"    // i2c_new_bus_info / i2c_free_bus_info

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#define RUN(f) do { printf("-- %s()\n", #f); f(); } while(0)

/* Runs `stmt` with stdout and stderr redirected, discarding the output.  Used
 * where the function under test is expected to log an error. */
#define QUIETLY(stmt) do { \
   fflush(stdout); fflush(stderr); \
   int _so = dup(fileno(stdout)); int _se = dup(fileno(stderr)); \
   FILE * _tmp = tmpfile(); \
   bool _redirected = (_so >= 0 && _se >= 0 && _tmp); \
   if (_redirected) { dup2(fileno(_tmp), fileno(stdout)); dup2(fileno(_tmp), fileno(stderr)); } \
   stmt; \
   fflush(stdout); fflush(stderr); \
   if (_redirected) { dup2(_so, fileno(stdout)); dup2(_se, fileno(stderr)); } \
   if (_so >= 0) close(_so); \
   if (_se >= 0) close(_se); \
   if (_tmp) fclose(_tmp); \
} while(0)

#define CK_INT(expr, expected) do { \
   total++; \
   long _a = (long)(expr); long _e = (long)(expected); \
   if (_a != _e) { failed++; \
      printf("FAIL  line %-4d  %s -> %ld, expected %ld\n", __LINE__, #expr, _a, _e); } \
} while(0)

/** The arrays are created by module initialization and exist from then on.
 *
 *  This is the invariant the asserts in i2c_bus_base.c and the absence of NULL
 *  checks there rely on, so it is asserted rather than assumed.
 */
static void test_global_arrays_created_by_init(void) {
   CK(all_i2c_buses != NULL);
   CK(removed_i2c_buses != NULL);
   CK_INT(all_i2c_buses->len, 0);
   CK_INT(removed_i2c_buses->len, 0);
}


/** A record is added, found by bus number and by index, and a second record for
 *  the same bus number is refused.
 */
static void test_add_and_find(void) {
   I2C_Bus_Info * b7 = i2c_new_bus_info(7);
   CK(i2c_add_businfo(b7) == true);
   CK_INT(all_i2c_buses->len, 1);
   CK(i2c_find_bus_info_by_busno(7) == b7);
   CK(i2c_get_bus_info_by_index(0) == b7);
   CK(i2c_get_bus_info_by_index(1) == NULL);       // index past the end
   CK(i2c_find_bus_info_by_busno(8) == NULL);

   // same record again, and a different record with the same bus number
   QUIETLY( CK(i2c_add_businfo(b7) == false) );
   I2C_Bus_Info * same_busno = i2c_new_bus_info(7);
   QUIETLY( CK(i2c_add_businfo(same_busno) == false) );
   CK_INT(all_i2c_buses->len, 1);
   i2c_free_bus_info(same_busno);
}


/** Removal moves the record to removed_i2c_buses.  It is not freed: only
 *  removed_i2c_buses owns its contents, which is what makes the move safe.
 */
static void test_remove_moves_record(void) {
   I2C_Bus_Info * b7 = i2c_find_bus_info_by_busno(7);
   CK(b7 != NULL);

   CK(i2c_remove_businfo(b7) == true);
   CK_INT(all_i2c_buses->len, 0);
   CK_INT(removed_i2c_buses->len, 1);
   CK(i2c_find_bus_info_by_busno(7) == NULL);
   CK(g_ptr_array_index(removed_i2c_buses, 0) == b7);

   // the record survived the move, and is flagged
   CK(b7->removed == true);
   CK_INT(b7->busno, 7);
   CK(memcmp(b7->marker, I2C_BUS_INFO_MARKER, 4) == 0);
}


/** i2c_discard_buses() empties all_i2c_buses without destroying it, and leaves
 *  removed_i2c_buses alone.  Leaving the removed records is intentional: a
 *  Display_Ref reaches its I2C_Bus_Info through dref->detail, so a removed
 *  record can still be referenced.  See the note in i2c_discard_buses().
 */
static void test_discard_empties_without_destroying(void) {
   CK(i2c_add_businfo(i2c_new_bus_info(9)) == true);
   CK(i2c_add_businfo(i2c_new_bus_info(10)) == true);
   CK_INT(all_i2c_buses->len, 2);
   guint removed_before = removed_i2c_buses->len;

   i2c_discard_buses();

   CK(all_i2c_buses != NULL);                        // not destroyed
   CK_INT(all_i2c_buses->len, 0);
   CK_INT(removed_i2c_buses->len, removed_before);   // untouched

   // the lookups tolerate the emptied array rather than needing a NULL check
   CK(i2c_find_bus_info_by_busno(9) == NULL);
   CK(i2c_get_bus_info_by_index(0) == NULL);
   CK(i2c_find_businfo_by_drm_connector_id(1234) == NULL);
}


int main(int argc, char ** argv) {
   setvbuf(stdout, NULL, _IONBF, 0);   // so output survives a crash

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

   // The global bus table.  init_i2c_bus_base() creates both arrays; the order
   // below matters, each test building on the state the previous one left.
   init_i2c_bus_base();
   RUN(test_global_arrays_created_by_init);
   RUN(test_add_and_find);
   RUN(test_remove_moves_record);
   RUN(test_discard_empties_without_destroying);
   terminate_i2c_bus_base();     // frees both arrays; must not double free

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
