/** @file test_udev_i2c_util.c
 *
 *  Standalone unit tests for src/util/udev_i2c_util.c.
 *
 *  udev_i2c_device_summary_busno() parses the bus number out of a device
 *  summary's sysname ("i2c-N"); it is checked here against constructed
 *  summaries.  get_i2c_devices_using_udev() enumerates real udev devices, so it
 *  is only checked for a well-formed result (a non-NULL array whose entries have
 *  valid bus numbers) since the actual contents depend on the host.
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/udev_util.h"       // Udev_Device_Summary
#include "util/udev_i2c_util.h"

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

static int busno_of(const char * sysname) {
   Udev_Device_Summary s = {0};
   s.sysname = (char *) sysname;
   return udev_i2c_device_summary_busno(&s);
}

int main(int argc, char ** argv) {
   // busno extracted from the "i2c-N" sysname
   CK_INT(busno_of("i2c-5"), 5);
   CK_INT(busno_of("i2c-0"), 0);
   CK_INT(busno_of("i2c-15"), 15);
   CK_INT(busno_of("i2c-abc"), -1);    // non-numeric suffix
   CK_INT(busno_of("foo"), -1);        // wrong prefix

   // enumeration returns a well-formed (possibly empty) list on any host
   GPtrArray * devs = get_i2c_devices_using_udev();
   CK(devs != NULL);
   if (devs) {
      for (guint i = 0; i < devs->len; i++) {
         Udev_Device_Summary * s = g_ptr_array_index(devs, i);
         CK(udev_i2c_device_summary_busno(s) >= 0);
      }
      free_udev_device_summaries(devs);
   }

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
