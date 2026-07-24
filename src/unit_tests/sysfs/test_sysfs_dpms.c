/** @file test_sysfs_dpms.c
 *
 *  Standalone unit tests for src/sysfs/sysfs_dpms.c: the DPMS state flag
 *  formatter, and dpms_check_drm_asleep_by_connector() for a connector
 *  name that cannot exist (its three sysfs attribute reads fail, and a
 *  missing "dpms" attribute is correctly reported as "asleep").
 *
 *  Not exercised: dpms_check_drm_asleep_by_businfo() and
 *  dpms_check_drm_asleep_by_dref() require a fully probed I2C_Bus_Info
 *  (drm_connector_found_by must not be DRM_CONNECTOR_NOT_CHECKED, which
 *  only real connector detection sets) and branch on the ambient
 *  XDG_SESSION_TYPE environment variable, making them unsafe to call with
 *  fabricated data portably.  dpms_is_x11_asleep() depends on a live X11
 *  session.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libsysfs unit test: it links the internal
 *  libsysfs/libi2c/libbase/libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sysfs/sysfs_dpms.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)


static void test_interpret_dpms_state(void) {
   char * s;

   s = interpret_dpms_state_t(0);
   CK(s != NULL && strlen(s) == 0);

   s = interpret_dpms_state_t(DPMS_SOME_DRM_ASLEEP);
   CK(s != NULL && strstr(s, "DPMS_SOME_DRM_ASLEEP") != NULL);

   s = interpret_dpms_state_t(DPMS_ALL_DRM_ASLEEP);
   CK(s != NULL && strstr(s, "DPMS_ALL_DRM_ASLEEP") != NULL);

   s = interpret_dpms_state_t(DPMS_SOME_DRM_ASLEEP | DPMS_ALL_DRM_ASLEEP);
   CK(s != NULL && strstr(s, "DPMS_SOME_DRM_ASLEEP") != NULL
                && strstr(s, "DPMS_ALL_DRM_ASLEEP")  != NULL);
}


static void test_dpms_check_drm_asleep_by_connector_bogus(void) {
   // connector doesn't exist: no "dpms" attribute to read, "!= On" -> asleep
   CK(dpms_check_drm_asleep_by_connector("definitely-bogus-connector-xyz"));
}


int main(int argc, char ** argv) {
   test_interpret_dpms_state();
   test_dpms_check_drm_asleep_by_connector_bogus();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
