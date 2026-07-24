/** @file test_sysfs_conflicting_drivers.c
 *
 *  Standalone unit tests for src/sysfs/sysfs_conflicting_drivers.c: the
 *  best-name fallback chain, the name-collecting/deduplicating and
 *  string-joining transforms over a fabricated GPtrArray of
 *  Sys_Conflicting_Driver records, the lifecycle functions, and
 *  collect_conflicting_drivers() for a bus number that does not exist
 *  (its /sys/bus/i2c/devices/i2c-N directory does not exist, so the
 *  directory scan simply finds nothing).
 *
 *  Not exercised: collect_conflicting_drivers_for_any_bus(), which scans
 *  every real DRM connector on the test host.
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

#include "sysfs/sysfs_conflicting_drivers.h"

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


static Sys_Conflicting_Driver * make_conflict(const char * name, const char * driver_module,
                                               const char * modalias)
{
   Sys_Conflicting_Driver * d = calloc(1, sizeof(Sys_Conflicting_Driver));
   d->n_nnnn        = strdup("4-0037");
   d->name          = name          ? strdup(name)          : NULL;
   d->driver_module = driver_module ? strdup(driver_module) : NULL;
   d->modalias      = modalias      ? strdup(modalias)      : NULL;
   return d;
}


static void test_best_conflicting_driver_name(void) {
   Sys_Conflicting_Driver * all_set = make_conflict("ddcci", "ddcci_mod", "i2c:ddcci");
   CK_STR(best_conflicting_driver_name(all_set), "ddcci");

   Sys_Conflicting_Driver * no_name = make_conflict(NULL, "ddcci_mod", "i2c:ddcci");
   CK_STR(best_conflicting_driver_name(no_name), "ddcci_mod");

   Sys_Conflicting_Driver * modalias_only = make_conflict(NULL, NULL, "i2c:ddcci");
   CK_STR(best_conflicting_driver_name(modalias_only), "i2c:ddcci");

   free_sys_conflicting_driver(all_set);
   free_sys_conflicting_driver(no_name);
   free_sys_conflicting_driver(modalias_only);
}


static void test_conflicting_driver_names(void) {
   GPtrArray * conflicts = g_ptr_array_new_with_free_func((GDestroyNotify) free_sys_conflicting_driver);
   g_ptr_array_add(conflicts, make_conflict("ddcci", NULL, NULL));
   g_ptr_array_add(conflicts, make_conflict("eeprom", NULL, NULL));
   g_ptr_array_add(conflicts, make_conflict("ddcci", NULL, NULL));   // duplicate name

   GPtrArray * names = conflicting_driver_names(conflicts);
   CK_INT(names->len, 2);   // deduplicated

   char * joined = conflicting_driver_names_string_t(conflicts);
   CK(strstr(joined, "ddcci")  != NULL);
   CK(strstr(joined, "eeprom") != NULL);

   g_ptr_array_free(names, true);
   free_conflicting_drivers(conflicts);
}


static void test_free_conflicting_drivers_null_safe(void) {
   free_conflicting_drivers(NULL);   // must not crash
   free_sys_conflicting_driver(NULL);   // must not crash
}


static void test_collect_conflicting_drivers_nonexistent_bus(void) {
   GPtrArray * conflicts = collect_conflicting_drivers(9999, -1);
   CK(conflicts != NULL);
   CK_INT(conflicts->len, 0);
   free_conflicting_drivers(conflicts);
}


int main(int argc, char ** argv) {
   test_best_conflicting_driver_name();
   test_conflicting_driver_names();
   test_free_conflicting_drivers_null_safe();
   test_collect_conflicting_drivers_nonexistent_bus();

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
