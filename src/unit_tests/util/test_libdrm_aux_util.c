/** @file test_libdrm_aux_util.c
 *
 *  Standalone unit tests for src/util/libdrm_aux_util.c.
 *
 *  Almost all of this module queries the DRM API of real devices and depends on
 *  the host's hardware.  The one host-independent function is drm_bus_type_name(),
 *  which maps a DRM bus-type constant to a short name; it is tested here.
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
#include <xf86drm.h>

#include "util/libdrm_aux_util.h"

static int total = 0;
static int failed = 0;

#define CK_STR(actual, expected) do { \
   total++; \
   const char * _a = (actual); const char * _e = (expected); \
   if (_a == NULL || strcmp(_a, _e) != 0) { failed++; \
      printf("FAIL  line %-4d  %s -> \"%s\", expected \"%s\"\n", __LINE__, #actual, \
             _a ? _a : "(null)", _e); } \
} while(0)

int main(int argc, char ** argv) {
   CK_STR(drm_bus_type_name(DRM_BUS_PCI), "pci");
   CK_STR(drm_bus_type_name(DRM_BUS_USB), "usb");
   CK_STR(drm_bus_type_name(DRM_BUS_PLATFORM), "platform");
   CK_STR(drm_bus_type_name(DRM_BUS_HOST1X), "host1x");
   CK_STR(drm_bus_type_name(200), "unrecognized");     // unknown bus type

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
