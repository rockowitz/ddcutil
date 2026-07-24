/** @file test_i2c_bus_selector.c
 *
 *  Standalone unit tests for i2c_find_bus_info_by_mfg_model_sn() in
 *  src/i2c/i2c_bus_selector.c.  The function searches the global
 *  all_i2c_buses array (normally populated by bus detection against real
 *  hardware); this test populates it directly with fabricated
 *  I2C_Bus_Info/Parsed_Edid records instead.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libi2c unit test: it links the internal libi2c/libbase/libutil
 *  convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/edid.h"

#include "base/i2c_bus_aux.h"
#include "base/i2c_bus_base.h"

#include "i2c/i2c_bus_selector.h"

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


// Fills in just the fields bus_info_matches_selector() reads: marker (so
// free_parsed_edid() will accept it), mfg_id, model_name, serial_ascii.
static Parsed_Edid * make_fake_edid(const char * mfg_id, const char * model_name,
                                     const char * serial_ascii)
{
   Parsed_Edid * edid = calloc(1, sizeof(Parsed_Edid));
   memcpy(edid->marker, EDID_MARKER_NAME, 4);
   g_strlcpy(edid->mfg_id,       mfg_id,       sizeof(edid->mfg_id));
   g_strlcpy(edid->model_name,   model_name,   sizeof(edid->model_name));
   g_strlcpy(edid->serial_ascii, serial_ascii, sizeof(edid->serial_ascii));
   return edid;
}


int main(int argc, char ** argv) {
   I2C_Bus_Info * b1 = i2c_new_bus_info(3);
   b1->edid = make_fake_edid("ACM", "Monitor One", "SN0001");

   I2C_Bus_Info * b2 = i2c_new_bus_info(5);
   b2->edid = make_fake_edid("DEL", "Monitor Two", "SN0002");

   I2C_Bus_Info * b3 = i2c_new_bus_info(7);   // no monitor: edid stays NULL

   all_i2c_buses = g_ptr_array_new_with_free_func((GDestroyNotify) i2c_free_bus_info);
   g_ptr_array_add(all_i2c_buses, b1);
   g_ptr_array_add(all_i2c_buses, b2);
   g_ptr_array_add(all_i2c_buses, b3);

   // exact single-field matches
   CK(i2c_find_bus_info_by_mfg_model_sn("ACM", NULL, NULL, 0) == b1);
   CK(i2c_find_bus_info_by_mfg_model_sn("DEL", NULL, NULL, 0) == b2);
   CK(i2c_find_bus_info_by_mfg_model_sn(NULL, "Monitor Two", NULL, 0) == b2);
   CK(i2c_find_bus_info_by_mfg_model_sn(NULL, NULL, "SN0001", 0) == b1);

   // combined fields, all must match
   CK(i2c_find_bus_info_by_mfg_model_sn("DEL", "Monitor Two", "SN0002", 0) == b2);

   // mismatched combination: mfg_id matches b2 but model_name does not
   CK(i2c_find_bus_info_by_mfg_model_sn("DEL", "Monitor One", NULL, 0) == NULL);

   // no such mfg_id
   CK(i2c_find_bus_info_by_mfg_model_sn("XXX", NULL, NULL, 0) == NULL);

   // bus with no EDID never matches a criterion that requires reading it
   CK(i2c_find_bus_info_by_mfg_model_sn("ANY", NULL, NULL, 0) == NULL);

   CK_INT(all_i2c_buses->len, 3);

   g_ptr_array_free(all_i2c_buses, true);
   all_i2c_buses = NULL;

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
