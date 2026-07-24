/** @file test_sysfs_base.c
 *
 *  Standalone unit tests for src/sysfs/sysfs_base.c.
 *
 *  Covers: the D-00hh predicate is_n_nnnn(), the Connector_Bus_Numbers
 *  lifecycle, the Sysfs_Connector_Names lifecycle and equality check (plus
 *  the invariant that connectors_having_edid is always a subset of
 *  all_connectors, checked against whatever /sys/class/drm actually looks
 *  like on the test host), the idempotence of sysfs_connector_directories_exist(),
 *  find_sysfs_drm_connector_name_by_edid() given an empty candidate list,
 *  is_sysfs_reliable_for_driver()'s deterministic branches (known-good
 *  drivers, and any driver name other than "nvidia" that isn't one of
 *  them) plus its force_sysfs_reliable/force_sysfs_unreliable test hooks,
 *  and the driver/class/name lookup functions' handling of an I2C bus
 *  number that does not exist.
 *
 *  Not exercised: functions that depend on the actual DRM connectors and
 *  drivers present on the test host (get_sys_drm_connector_name_by_*(),
 *  all_sys_drm_connectors_have_connector_id_direct(),
 *  is_sysfs_reliable_for_driver("nvidia"), the possibly_write_detect_to_status_*()
 *  family), and search_all_businfo_records_by_connector_name() (requires
 *  the global all_i2c_buses array).
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  This is a libsysfs unit test: it links the internal
 *  libsysfs/libi2c/libbase/libutil convenience libraries directly.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/string_util.h"

#include "sysfs/sysfs_base.h"

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

#define NONEXISTENT_BUSNO 9999


static void test_is_n_nnnn(void) {
   CK(is_n_nnnn("/sys/bus/i2c/devices/i2c-4", "4-0050"));
   CK(is_n_nnnn("/sys/bus/i2c/devices/i2c-4", "4-0037"));
   CK(!is_n_nnnn("/sys/bus/i2c/devices/i2c-4", "not-a-match"));
   CK(!is_n_nnnn("/sys/bus/i2c/devices/i2c-4", "i2c-4"));
}


static void test_connector_bus_numbers_lifecycle(void) {
   Connector_Bus_Numbers * cbn = calloc(1, sizeof(Connector_Bus_Numbers));
   cbn->i2c_busno = 5;
   cbn->base_busno = -1;
   cbn->connector_id = 3;
   cbn->name = strdup("card0-DP-1");
   CK_INT(cbn->i2c_busno, 5);
   CK_STR(cbn->name, "card0-DP-1");
   free_connector_bus_numbers(cbn);   // must not crash
}


static Sysfs_Connector_Names make_names(const char * const * all, int all_ct,
                                         const char * const * edid, int edid_ct)
{
   Sysfs_Connector_Names names;
   names.all_connectors         = g_ptr_array_new_with_free_func(g_free);
   names.connectors_having_edid = g_ptr_array_new_with_free_func(g_free);
   for (int i = 0; i < all_ct; i++)
      g_ptr_array_add(names.all_connectors, g_strdup(all[i]));
   for (int i = 0; i < edid_ct; i++)
      g_ptr_array_add(names.connectors_having_edid, g_strdup(edid[i]));
   return names;
}


static void test_sysfs_connector_names(void) {
   const char * all[]  = {"card0-DP-1", "card0-HDMI-A-1"};
   const char * edid[] = {"card0-DP-1"};

   Sysfs_Connector_Names n1 = make_names(all, 2, edid, 1);
   Sysfs_Connector_Names n2 = make_names(all, 2, edid, 1);
   CK(sysfs_connector_names_equal(n1, n2));

   Sysfs_Connector_Names copy = copy_sysfs_connector_names_struct(n1);
   CK(sysfs_connector_names_equal(n1, copy));
   CK_INT(copy.all_connectors->len, 2);
   CK_INT(copy.connectors_having_edid->len, 1);

   const char * different[] = {"card0-DP-1", "card1-HDMI-A-1"};
   Sysfs_Connector_Names n3 = make_names(different, 2, edid, 1);
   CK(!sysfs_connector_names_equal(n1, n3));

   free_sysfs_connector_names_contents(n1);
   free_sysfs_connector_names_contents(n2);
   free_sysfs_connector_names_contents(n3);
   free_sysfs_connector_names_contents(copy);
}


// Portable regardless of what /sys/class/drm actually contains on the test
// host: connectors_having_edid must always be a subset of all_connectors,
// and the function must be idempotent between two calls made in quick
// succession (no hardware change in between).
static void test_get_sysfs_drm_connector_names_invariants(void) {
   Sysfs_Connector_Names n1 = get_sysfs_drm_connector_names();
   for (guint i = 0; i < n1.connectors_having_edid->len; i++) {
      const char * name = g_ptr_array_index(n1.connectors_having_edid, i);
      bool found = false;
      for (guint j = 0; j < n1.all_connectors->len; j++) {
         if (streq(name, g_ptr_array_index(n1.all_connectors, j))) {
            found = true;
            break;
         }
      }
      CK(found);
   }

   Sysfs_Connector_Names n2 = get_sysfs_drm_connector_names();
   CK(sysfs_connector_names_equal(n1, n2));

   free_sysfs_connector_names_contents(n1);
   free_sysfs_connector_names_contents(n2);
}


static void test_sysfs_connector_directories_exist_idempotent(void) {
   bool first  = sysfs_connector_directories_exist();
   bool second = sysfs_connector_directories_exist();
   CK(first == second);
}


static void test_find_sysfs_drm_connector_name_by_edid_empty(void) {
   GPtrArray * empty = g_ptr_array_new();
   Byte edid[128] = {0};
   char * result = find_sysfs_drm_connector_name_by_edid(empty, edid);
   CK(result == NULL);
   g_ptr_array_free(empty, true);
}


static void test_is_sysfs_reliable_for_driver(void) {
   CK(is_sysfs_reliable_for_driver("i915"));
   CK(is_sysfs_reliable_for_driver("amdgpu"));
   CK(is_sysfs_reliable_for_driver("radeon"));
   CK(is_sysfs_reliable_for_driver("nouveau"));
   CK(is_sysfs_reliable_for_driver("xe"));
   CK(!is_sysfs_reliable_for_driver("definitely-bogus-driver-xyz"));

   force_sysfs_reliable = true;
   CK(is_sysfs_reliable_for_driver("definitely-bogus-driver-xyz"));
   force_sysfs_reliable = false;

   force_sysfs_unreliable = true;
   CK(!is_sysfs_reliable_for_driver("i915"));
   force_sysfs_unreliable = false;

   // restore: neither force flag set
   CK(is_sysfs_reliable_for_driver("i915"));
}


static void test_nonexistent_busno_lookups(void) {
   char * driver = get_i2c_sysfs_driver_by_busno(NONEXISTENT_BUSNO);
   CK(driver == NULL);

   char * name = get_i2c_device_sysfs_name(NONEXISTENT_BUSNO);
   CK(name == NULL);

   uint32_t class = get_i2c_device_sysfs_class(NONEXISTENT_BUSNO);
   CK_INT(class, 0);

   // busno doesn't exist: no name, class 0 -> ignorable
   CK(sysfs_is_ignorable_i2c_device(NONEXISTENT_BUSNO));

   // no driver found for a nonexistent bus -> not a known-reliable driver
   CK(!is_sysfs_reliable_for_busno(NONEXISTENT_BUSNO));

   char * adapter_driver = find_adapter_and_get_driver("/sys/bus/i2c/devices/i2c-9999", -1);
   CK(adapter_driver == NULL);

   char * adapter_path = sysfs_find_adapter("/sys/bus/i2c/devices/i2c-9999");
   CK(adapter_path == NULL);
}


int main(int argc, char ** argv) {
   test_is_n_nnnn();
   test_connector_bus_numbers_lifecycle();
   test_sysfs_connector_names();
   test_get_sysfs_drm_connector_names_invariants();
   test_sysfs_connector_directories_exist_idempotent();
   test_find_sysfs_drm_connector_name_by_edid_empty();
   test_is_sysfs_reliable_for_driver();
   test_nonexistent_busno_lookups();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
