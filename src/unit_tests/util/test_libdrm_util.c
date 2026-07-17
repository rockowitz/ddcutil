/** @file test_libdrm_util.c
 *
 *  Standalone unit tests for the table-lookup functions in
 *  src/util/libdrm_util.c: mapping a DRM connector-type name to its numeric
 *  type, and mapping connector-type and connection-status values back to their
 *  short names and titles.  The functions that report live drmMode* structures
 *  require a DRM device and are not exercised.
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

#include "util/coredefs_base.h"
#include "util/data_structures.h"
#include "util/libdrm_util.h"

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

int main(int argc, char ** argv) {
   // name -> connector type (searches titles, case-insensitive)
   CK_INT(lookup_drm_connector_type("DP"), DRM_MODE_CONNECTOR_DisplayPort);
   CK_INT(lookup_drm_connector_type("VGA"), DRM_MODE_CONNECTOR_VGA);
   CK_INT(lookup_drm_connector_type("HDMI"), DRM_MODE_CONNECTOR_HDMIA);
   CK_INT(lookup_drm_connector_type("hdmi"), DRM_MODE_CONNECTOR_HDMIA);   // ignore case
   CK_INT(lookup_drm_connector_type("eDP"), DRM_MODE_CONNECTOR_eDP);
   CK_INT(lookup_drm_connector_type("nonsense"), -1);                     // default

   // connector type -> title (the short descriptive string)
   CK_STR(drm_connector_type_title(DRM_MODE_CONNECTOR_DisplayPort), "DP");
   CK_STR(drm_connector_type_title(DRM_MODE_CONNECTOR_VGA), "VGA");
   CK_STR(drm_connector_type_title(DRM_MODE_CONNECTOR_DVII), "DVI-I");
   CK_STR(drm_connector_type_title(DRM_MODE_CONNECTOR_eDP), "eDP");
   CK(drm_connector_type_title(200) == NULL);                            // not in table

   // connector type -> name (the stringified enum constant)
   CK_STR(drm_connector_type_name(DRM_MODE_CONNECTOR_DisplayPort),
          "DRM_MODE_CONNECTOR_DisplayPort");

   // connection status -> title / name
   CK_STR(connector_status_title(DRM_MODE_CONNECTED), "connected");
   CK_STR(connector_status_title(DRM_MODE_DISCONNECTED), "disconnected");
   CK_STR(connector_status_name(DRM_MODE_CONNECTED), "DRM_MODE_CONNECTED");

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
