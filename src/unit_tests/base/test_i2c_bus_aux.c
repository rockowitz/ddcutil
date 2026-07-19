/** @file test_i2c_bus_aux.c
 *
 *  Standalone unit tests for the host-independent functions of
 *  src/base/i2c_bus_aux.c: the drm-connector "found by" and x37-detection state
 *  name lookups, and the I2C_Bus_Info allocate/copy/free lifecycle.  The sysfs
 *  probing functions are not exercised.
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

#include "base/i2c_bus_base.h"
#include "base/i2c_bus_aux.h"

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

static void test_name_lookups(void) {
   CK_STR(drm_connector_found_by_name(DRM_CONNECTOR_NOT_CHECKED), "DRM_CONNECTOR_NOT_CHECKED");
   CK_STR(drm_connector_found_by_name(DRM_CONNECTOR_NOT_FOUND), "DRM_CONNECTOR_NOT_FOUND");
   CK_STR(drm_connector_found_by_name(DRM_CONNECTOR_FOUND_BY_BUSNO), "DRM_CONNECTOR_FOUND_BY_BUSNO");
   CK_STR(drm_connector_found_by_name(DRM_CONNECTOR_FOUND_BY_EDID), "DRM_CONNECTOR_FOUND_BY_EDID");

   CK_STR(x37_detection_state_name(X37_Not_Recorded), "X37_Not_Recorded");
   CK_STR(x37_detection_state_name(X37_Not_Detected), "X37_Not_Detected");
   CK_STR(x37_detection_state_name(X37_Detected), "X37_Detected");
}

static void test_bus_info_lifecycle(void) {
   I2C_Bus_Info * bi = i2c_new_bus_info(5);
   CK(bi != NULL);
   CK_INT(bi->busno, 5);
   CK_INT(bi->drm_connector_found_by, DRM_CONNECTOR_NOT_CHECKED);

   I2C_Bus_Info * cp = i2c_copy_bus_info(bi);
   CK(cp != NULL && cp != bi);
   CK_INT(cp->busno, 5);

   i2c_free_bus_info(cp);
   i2c_free_bus_info(bi);
}

int main(int argc, char ** argv) {
   test_name_lookups();
   test_bus_info_lifecycle();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
