/** @file test_udev_util.c
 *
 *  Standalone unit tests for src/util/udev_util.c.
 *
 *  filter_device_summaries() removes, in place, the summaries that fail a
 *  predicate; it is checked here against constructed summaries and the NULL
 *  argument cases.  summarize_udev_subsystem_devices() enumerates real udev
 *  devices, so it is only checked for a well-formed (non-NULL) result.
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

#include "util/udev_util.h"

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

// keep summaries whose sysname starts with "i2c-"
static bool keep_i2c(Udev_Device_Summary * s) {
   return s->sysname && strncmp(s->sysname, "i2c-", 4) == 0;
}

int main(int argc, char ** argv) {
   Udev_Device_Summary a = {0}; a.sysname = "i2c-1";
   Udev_Device_Summary b = {0}; b.sysname = "i2c-2";
   Udev_Device_Summary c = {0}; c.sysname = "usbmisc0";

   // no free func: filter removes pointers without freeing the (stack) structs
   GPtrArray * arr = g_ptr_array_new();
   g_ptr_array_add(arr, &a);
   g_ptr_array_add(arr, &b);
   g_ptr_array_add(arr, &c);

   GPtrArray * result = filter_device_summaries(arr, keep_i2c);
   CK(result == arr);                 // filters in place, returns same array
   CK_INT(arr->len, 2);               // only the two i2c- entries remain
   CK(g_ptr_array_index(arr, 0) == &a);
   CK(g_ptr_array_index(arr, 1) == &b);

   // NULL predicate leaves the array unchanged
   CK(filter_device_summaries(arr, NULL) == arr);
   CK_INT(arr->len, 2);

   // NULL summaries -> NULL
   CK(filter_device_summaries(NULL, keep_i2c) == NULL);

   g_ptr_array_free(arr, FALSE);

   // real enumeration returns a well-formed array on any host
   GPtrArray * summaries = summarize_udev_subsystem_devices("i2c-dev");
   CK(summaries != NULL);
   if (summaries)
      free_udev_device_summaries(summaries);

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
