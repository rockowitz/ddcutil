/** @file test_usb_displays.c
 *
 *  Standalone unit tests for the pure subset of src/usb/usb_displays.c:
 *  create_usb_monitor_vcp_rec(), create_usb_monitor_info() (both simple
 *  allocators, no hardware I/O), dbgrpt_usb_monitor_info() (a smoke test
 *  driven with a hand-built Usb_Monitor_Info), get_usb_open_errors() and
 *  discard_usb_monitor_list() before any monitor probe has ever run (the
 *  module-level usb_monitors/usb_open_errors caches start out NULL).
 *
 *  Every other function in this file either probes real hiddev devices
 *  (get_usb_monitor_list()), looks up a real udev device
 *  (is_possible_monitor_by_hiddev_name(), check_usb_monitor()), or
 *  searches the usb_monitors cache populated only by a real probe
 *  (usb_find_monitor_by_dh()/_by_dref(), usb_get_parsed_edid_by_dh(),
 *  usb_show_active_display_by_dref(), etc -- these all either assert
 *  usb_monitors is non-NULL or require a valid Display_Handle/Display_Ref
 *  tied to a real device), and so are out of scope for these unit tests.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libcommon unit test: it links the internal libusb/libbase/
 *  libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/hiddev.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "usb/usb_displays.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)

/* Runs statement `stmt` with stdout (fd 1) redirected to a temporary file,
 * discarding the captured output. */
#define QUIETLY(stmt) do { \
   fflush(stdout); \
   int _saved = dup(fileno(stdout)); \
   FILE * _tmp = tmpfile(); \
   dup2(fileno(_tmp), fileno(stdout)); \
   stmt; \
   fflush(stdout); \
   dup2(_saved, fileno(stdout)); \
   close(_saved); \
   fclose(_tmp); \
} while(0)


static void test_create_usb_monitor_vcp_rec(void) {
   Usb_Monitor_Vcp_Rec * vcprec = create_usb_monitor_vcp_rec(0x10);
   CK(vcprec != NULL);
   CK(memcmp(vcprec->marker, USB_MONITOR_VCP_REC_MARKER, 4) == 0);
   CK(vcprec->vcp_code == 0x10);
   CK(vcprec->rinfo == NULL);
   CK(vcprec->finfo == NULL);
   CK(vcprec->uref == NULL);
   free(vcprec);   // no rinfo/finfo/uref allocated, safe to free directly
}


static void test_create_usb_monitor_info(void) {
   Usb_Monitor_Info * moninfo = create_usb_monitor_info("/dev/usb/hiddev3");
   CK(moninfo != NULL);
   CK(memcmp(moninfo->marker, USB_MONITOR_INFO_MARKER, 4) == 0);
   CK_STR(moninfo->hiddev_device_name, "/dev/usb/hiddev3");
   CK(moninfo->edid == NULL);
   CK(moninfo->hiddev_devinfo == NULL);
   for (int i = 0; i < 256; i++)
      CK(moninfo->vcp_codes[i] == NULL);
   free(moninfo->hiddev_device_name);
   free(moninfo);
}


static void test_dbgrpt_usb_monitor_info_smoke(void) {
   Usb_Monitor_Info moninfo = {0};
   memcpy(moninfo.marker, USB_MONITOR_INFO_MARKER, 4);
   moninfo.hiddev_device_name = "/dev/usb/hiddev3";
   // edid and hiddev_devinfo left NULL: dbgrpt_usb_monitor_info() only
   // prints them with "%p", it never dereferences them
   // vcp_codes[] left all-NULL

   QUIETLY( dbgrpt_usb_monitor_info(&moninfo, 0) );
   CK(true);   // reaching here without crashing is the test
}


static void test_get_usb_open_errors_before_probe(void) {
   // no probe (get_usb_monitor_list()) has run in this process, so the
   // module-level cache is still NULL
   CK(get_usb_open_errors() == NULL);
}


static void test_discard_usb_monitor_list_noop_when_uninitialized(void) {
   discard_usb_monitor_list();   // must not crash when nothing was ever cached
   CK(true);
   CK(get_usb_open_errors() == NULL);   // still uninitialized
}


int main(int argc, char ** argv) {
   test_create_usb_monitor_vcp_rec();
   test_create_usb_monitor_info();
   test_dbgrpt_usb_monitor_info_smoke();
   // order matters: these must run before anything that could trigger a
   // real probe of usb_monitors/usb_open_errors
   test_get_usb_open_errors_before_probe();
   test_discard_usb_monitor_list_noop_when_uninitialized();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
