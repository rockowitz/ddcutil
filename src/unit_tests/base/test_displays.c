/** @file test_displays.c
 *
 *  Standalone unit tests for the pure functions in src/base/displays.c: the
 *  io-mode and display-id-type names, the DDCA_IO_Path constructors / equality /
 *  hash, the Display_Identifier constructors and repr, and the Display_Selector
 *  new/empty/only-busno predicates and the identifier-to-selector conversion.
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

#include "public/ddcutil_types.h"
#include "util/coredefs_base.h"    // Byte
#include "base/displays.h"

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

static void test_names(void) {
   CK_STR(io_mode_name(DDCA_IO_I2C), "DDCA_IO_I2C");
   CK_STR(io_mode_name(DDCA_IO_USB), "DDCA_IO_USB");
   CK(io_mode_name(99) == NULL);              // out of range

   CK_STR(display_id_type_name(DISP_ID_BUSNO),  "DISP_ID_BUSNO");
   CK_STR(display_id_type_name(DISP_ID_EDID),   "DISP_ID_EDID");
   CK_STR(display_id_type_name(DISP_ID_HIDDEV), "DISP_ID_HIDDEV");
}

static void test_io_path(void) {
   DDCA_IO_Path p5 = i2c_io_path(5);
   CK_INT(p5.io_mode, DDCA_IO_I2C);
   CK_INT(p5.path.i2c_busno, 5);

   DDCA_IO_Path u3 = usb_io_path(3);
   CK_INT(u3.io_mode, DDCA_IO_USB);
   CK_INT(u3.path.hiddev_devno, 3);

   // equality: same mode and number
   CK(dpath_eq(i2c_io_path(5), i2c_io_path(5)) == true);
   CK(dpath_eq(i2c_io_path(5), i2c_io_path(6)) == false);
   CK(dpath_eq(i2c_io_path(5), usb_io_path(5)) == false);   // different mode

   // hash = io_mode * 100 + number
   CK_INT(dpath_hash(i2c_io_path(5)), 5);
   CK_INT(dpath_hash(usb_io_path(3)), 103);
}

static void test_identifiers(void) {
   Display_Identifier * d = create_busno_display_identifier(5);
   CK(memcmp(d->marker, "DPID", 4) == 0);
   CK_INT(d->id_type, DISP_ID_BUSNO);
   CK_INT(d->busno, 5);
   CK(did_repr(d) != NULL);
   free_display_identifier(d);

   d = create_dispno_display_identifier(3);
   CK_INT(d->id_type, DISP_ID_DISPNO);
   CK_INT(d->dispno, 3);
   free_display_identifier(d);

   d = create_mfg_model_sn_display_identifier("DEL", "Model1", "SN123");
   CK_INT(d->id_type, DISP_ID_MONSER);
   CK_STR(d->mfg_id, "DEL");
   CK_STR(d->model_name, "Model1");
   CK_STR(d->serial_ascii, "SN123");
   free_display_identifier(d);

   Byte edid[128];
   for (int i = 0; i < 128; i++) edid[i] = (Byte) i;
   d = create_edid_display_identifier(edid);
   CK_INT(d->id_type, DISP_ID_EDID);
   CK(memcmp(d->edidbytes, edid, 128) == 0);
   free_display_identifier(d);

   d = create_usb_display_identifier(1, 2);
   CK_INT(d->id_type, DISP_ID_USB);
   CK_INT(d->usb_bus, 1);
   CK_INT(d->usb_device, 2);
   free_display_identifier(d);

   d = create_usb_hiddev_display_identifier(4);
   CK_INT(d->id_type, DISP_ID_HIDDEV);
   CK_INT(d->hiddev_devno, 4);
   free_display_identifier(d);
}

static void test_selectors(void) {
   Display_Selector * dsel = dsel_new();
   CK(memcmp(dsel->marker, "DSEL", 4) == 0);
   CK(dsel_is_empty(dsel) == true);
   CK(dsel_only_busno(dsel) == false);        // busno is -1
   dsel_free(dsel);

   // identifier -> selector conversion
   Display_Identifier * dbus = create_busno_display_identifier(7);
   Display_Selector * s1 = display_id_to_dsel(dbus);
   CK_INT(s1->busno, 7);
   CK(dsel_is_empty(s1) == false);
   CK(dsel_only_busno(s1) == true);
   dsel_free(s1);
   free_display_identifier(dbus);

   Display_Identifier * ddisp = create_dispno_display_identifier(2);
   Display_Selector * s2 = display_id_to_dsel(ddisp);
   CK_INT(s2->dispno, 2);
   CK(dsel_only_busno(s2) == false);          // set by dispno, not busno
   dsel_free(s2);
   free_display_identifier(ddisp);
}

int main(int argc, char ** argv) {
   test_names();
   test_io_path();
   test_identifiers();
   test_selectors();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
