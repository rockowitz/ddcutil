/** @file test_ddc_phantom_displays.c
 *
 *  Standalone unit tests for src/ddc/ddc_phantom_displays.c:
 *  drefs_edid_equal() and filter_phantom_displays(), driven with
 *  fabricated Display_Ref instances using a nonexistent I2C bus number
 *  (9999), so any real sysfs probing inside is_phantom_display() (a
 *  static/internal helper exercised only indirectly through
 *  filter_phantom_displays()) gracefully finds nothing and returns false,
 *  the same "nonexistent path" pattern used and verified safe throughout
 *  the src/unit_tests/sysfs suite -- no real hardware or live sysfs state
 *  is touched or required.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: ddc source files cross-reference each other
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

#include "ddc/ddc_phantom_displays.h"

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


static Parsed_Edid * make_edid(const char * mfg_id, const char * model_name,
                                uint32_t serial_binary, Byte fill_byte)
{
   Parsed_Edid * edid = calloc(1, sizeof(Parsed_Edid));
   memcpy(edid->marker, EDID_MARKER_NAME, 4);
   memset(edid->bytes, fill_byte, 128);
   STRLCPY(edid->mfg_id, mfg_id, sizeof(edid->mfg_id));
   STRLCPY(edid->model_name, model_name, sizeof(edid->model_name));
   edid->serial_binary = serial_binary;
   return edid;
}


// Display_Ref fields exercised here are set directly, bypassing real
// display detection; the ref is never opened, so a bogus busno is safe.
static Display_Ref * make_dref(int busno, int dispno, Parsed_Edid * edid) {
   Display_Ref * dref = create_bus_display_ref(busno);
   dref->dispno = dispno;
   dref->pedid = edid;
   if (dispno >= 0) {
      // filter_phantom_displays() unconditionally dereferences
      // dref->detail for every valid (dispno >= 0) display ref.
      dref->detail = i2c_new_bus_info(busno);
   }
   return dref;
}


static void test_drefs_edid_equal(void) {
   Parsed_Edid * e1 = make_edid("ACM", "Model1", 0x1111, 0xAA);
   Parsed_Edid * e2 = make_edid("ACM", "Model1", 0x1111, 0xAA);   // identical bytes
   Parsed_Edid * e3 = make_edid("ACM", "Model1", 0x1111, 0xBB);   // differing bytes

   Display_Ref * d1 = make_dref(9001, 1, e1);
   Display_Ref * d2 = make_dref(9002, 2, e2);
   Display_Ref * d3 = make_dref(9003, 3, e3);

   CK(drefs_edid_equal(d1, d2));
   CK(!drefs_edid_equal(d1, d3));

   // no EDID on one side -> not equal, must not crash
   Display_Ref * d4 = make_dref(9004, 4, NULL);
   CK(!drefs_edid_equal(d1, d4));
}


static void test_filter_phantom_displays_disabled(void) {
   bool saved = detect_phantom_displays;
   detect_phantom_displays = false;

   GPtrArray * displays = g_ptr_array_new();
   Display_Ref * valid   = make_dref(9010, 1,  make_edid("ACM", "M", 1, 0x01));
   Display_Ref * invalid = make_dref(9011, -1, make_edid("ACM", "M", 1, 0x01));
   g_ptr_array_add(displays, valid);
   g_ptr_array_add(displays, invalid);

   CK(!filter_phantom_displays(displays));

   detect_phantom_displays = saved;
   g_ptr_array_free(displays, true);
}


static void test_filter_phantom_displays_too_few(void) {
   GPtrArray * displays = g_ptr_array_new();
   Display_Ref * only = make_dref(9020, 1, make_edid("ACM", "M", 1, 0x01));
   g_ptr_array_add(displays, only);

   CK(!filter_phantom_displays(displays));   // len <= 1

   g_ptr_array_free(displays, true);
}


static void test_filter_phantom_displays_no_invalid(void) {
   GPtrArray * displays = g_ptr_array_new();
   Display_Ref * d1 = make_dref(9030, 1, make_edid("ACM", "M1", 1, 0x01));
   Display_Ref * d2 = make_dref(9031, 2, make_edid("ACM", "M2", 2, 0x02));
   g_ptr_array_add(displays, d1);
   g_ptr_array_add(displays, d2);

   CK(!filter_phantom_displays(displays));   // no invalid displays present

   g_ptr_array_free(displays, true);
}


static void test_filter_phantom_displays_with_invalid(void) {
   GPtrArray * displays = g_ptr_array_new();
   Parsed_Edid * shared_style_edid = make_edid("ACM", "SameModel", 42, 0x07);
   Display_Ref * valid   = make_dref(9040, 1,  make_edid("ACM", "OtherModel", 99, 0x09));
   Display_Ref * invalid = make_dref(9041, -1, shared_style_edid);
   g_ptr_array_add(displays, valid);
   g_ptr_array_add(displays, invalid);

   // Returns true whenever any invalid display is present, independent of
   // whether a real phantom match was actually found (is_phantom_display()'s
   // sysfs probe against nonexistent bus 9041 gracefully returns false, so
   // no match occurs here -- this only exercises the "at least one invalid
   // display" return path).
   CK(filter_phantom_displays(displays));
   // no real match was found, so the invalid ref is not reclassified
   CK_INT(invalid->dispno, -1);

   g_ptr_array_free(displays, true);
}


int main(int argc, char ** argv) {
   test_drefs_edid_equal();
   test_filter_phantom_displays_disabled();
   test_filter_phantom_displays_too_few();
   test_filter_phantom_displays_no_invalid();
   test_filter_phantom_displays_with_invalid();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
