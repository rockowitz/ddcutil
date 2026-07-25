/** @file test_dw_dref.c
 *
 *  Standalone unit tests for src/dw/dw_dref.c: dw_add_display_ref(),
 *  dw_add_display_by_businfo(), and dw_remove_display_by_businfo(). These
 *  modify the global all_display_refs list (extern in base/displays.h),
 *  populated here directly rather than via real display detection.
 *
 *  dw_add_display_by_businfo() only performs real DDC communication
 *  (ddc_initial_checks_by_dref(), via dref_lock()) when the I2C_BUS_ADDR_X37
 *  flag is set on the I2C_Bus_Info; every fabricated businfo here leaves
 *  that flag unset, so the function takes its no-DDC-check path deterministically.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: dw source files cross-reference each other
 *  and the rest of the ddcutil core extensively, so it links the full
 *  top-level libcommon convenience library (the same aggregate the
 *  ddcutil executable itself links) rather than a minimal per-directory
 *  library set.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/coredefs.h"
#include "util/edid.h"

#include "base/displays.h"
#include "base/i2c_bus_aux.h"
#include "base/i2c_bus_base.h"

#include "dw/dw_dref.h"

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


static Parsed_Edid * make_valid_edid(Byte fill_byte) {
   Byte raw[128];
   memset(raw, fill_byte, 128);
   raw[0] = 0x00; raw[1] = raw[2] = raw[3] = raw[4] = raw[5] = raw[6] = 0xff; raw[7] = 0x00;
   int sum = 0;
   for (int i = 0; i < 127; i++)
      sum += raw[i];
   raw[127] = (Byte)((256 - (sum % 256)) % 256);
   return create_parsed_edid2(raw, "TEST");
}


static bool ptr_array_contains(GPtrArray * a, gpointer p) {
   for (int i = 0; i < a->len; i++)
      if (g_ptr_array_index(a, i) == p)
         return true;
   return false;
}


static void test_dw_add_display_ref(void) {
   all_display_refs = g_ptr_array_new();

   Display_Ref * dref = create_bus_display_ref(101);
   dw_add_display_ref(dref);

   CK_INT(all_display_refs->len, 1);
   CK(ptr_array_contains(all_display_refs, dref));

   g_ptr_array_free(all_display_refs, true);
   all_display_refs = NULL;
}


static void test_dw_add_display_by_businfo_no_edid(void) {
   I2C_Bus_Info * businfo = i2c_new_bus_info(102);
   businfo->flags |= I2C_BUS_PROBED;
   businfo->edid = NULL;

   all_display_refs = g_ptr_array_new();
   Display_Ref * dref = dw_add_display_by_businfo(businfo);
   CK(dref == NULL);
   CK_INT(all_display_refs->len, 0);

   g_ptr_array_free(all_display_refs, true);
   all_display_refs = NULL;
   i2c_free_bus_info(businfo);
}


static void test_dw_add_and_remove_display_by_businfo(void) {
   const int busno = 103;
   I2C_Bus_Info * businfo = i2c_new_bus_info(busno);
   businfo->flags |= I2C_BUS_PROBED;   // I2C_BUS_ADDR_X37 deliberately not set
   businfo->edid = make_valid_edid(0x77);
   Byte edid_bytes_copy[128];
   memcpy(edid_bytes_copy, businfo->edid->bytes, 128);

   all_display_refs = g_ptr_array_new();

   Display_Ref * dref = dw_add_display_by_businfo(businfo);
   CK(dref != NULL);
   if (dref) {
      CK(dref->io_path.io_mode == DDCA_IO_I2C);
      CK_INT(dref->io_path.path.i2c_busno, busno);
      CK(dref->pedid != NULL && memcmp(dref->pedid->bytes, edid_bytes_copy, 128) == 0);
      CK(dref->mmid != NULL);
      CK(dref->detail == businfo);
      CK(dref->flags & DREF_DDC_IS_MONITOR);
      // I2C_BUS_ADDR_X37 was not set, so ddc_initial_checks_by_dref() was
      // never called and DREF_DDC_COMMUNICATION_WORKING stays clear
      CK(!(dref->flags & DREF_DDC_COMMUNICATION_WORKING));
      CK_INT(dref->dispno, DISPNO_INVALID);
      CK(ptr_array_contains(all_display_refs, dref));
   }

   // dw_remove_display_by_businfo() looks up the dref via
   // GET_DREF_BY_BUSNO(busno, /*ignore_invalid*/ true), which skips any
   // dref with dispno <= 0 -- exactly the DISPNO_INVALID state just
   // asserted above. So with no further hardware-dependent step to make
   // DDC communication "working", removal of this still-invalid dref is
   // not found.
   Display_Ref * removed_while_invalid = dw_remove_display_by_businfo(businfo);
   CK(removed_while_invalid == NULL);
   if (dref)
      CK(!dref->disconnected);

   // Simulate the dref having since become a valid, numbered display (as
   // dw_add_display_by_businfo() itself would do had DDC communication been
   // confirmed working) so the removal path that actually marks a dref
   // disconnected can be exercised.
   if (dref)
      dref->dispno = 5;
   Display_Ref * removed = dw_remove_display_by_businfo(businfo);
   CK(removed == dref);
   if (removed) {
      CK(removed->disconnected);
      CK(removed->detail == NULL);
   }

   g_ptr_array_free(all_display_refs, true);
   all_display_refs = NULL;
   // businfo->edid was freed by i2c_reset_bus_info() inside dw_remove_display_by_businfo()
   i2c_free_bus_info(businfo);
}


static void test_dw_remove_display_by_businfo_not_found(void) {
   I2C_Bus_Info * businfo = i2c_new_bus_info(104);   // never added via dw_add_display_ref

   all_display_refs = g_ptr_array_new();
   Display_Ref * removed = dw_remove_display_by_businfo(businfo);
   CK(removed == NULL);

   g_ptr_array_free(all_display_refs, true);
   all_display_refs = NULL;
   i2c_free_bus_info(businfo);
}


int main(int argc, char ** argv) {
   test_dw_add_display_ref();
   test_dw_add_display_by_businfo_no_edid();
   test_dw_add_and_remove_display_by_businfo();
   test_dw_remove_display_by_businfo_not_found();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
