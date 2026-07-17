/** @file test_sysfs_i2c_util.c
 *
 *  Standalone unit tests for src/util/sysfs_i2c_util.c.
 *
 *  is_module_loaded_using_sysfs() checks for /sys/module/<name>.  A clearly
 *  bogus module name is never loaded; a positive case is chosen by reading an
 *  actual entry from /sys/module at runtime, so the test adapts to the host.
 *  get_video_adapter_devices() depends on the host's hardware and is not
 *  exercised.
 *
 *  Prints one line per failing check and a summary; exit status is 0 if all
 *  checks pass, 1 otherwise.
 *
 *  Not a libddcutil client -- it links the internal util convenience library
 *  directly, since these symbols are not exported from libddcutil.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <dirent.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/sysfs_i2c_util.h"

static int total = 0;
static int failed = 0;

#define CK(cond) do { \
   total++; \
   if (!(cond)) { failed++; printf("FAIL  line %-4d  %s\n", __LINE__, #cond); } \
} while(0)

int main(int argc, char ** argv) {
   // a bogus module is never loaded
   CK(is_module_loaded_using_sysfs("no_such_module_zqx_999") == false);

   // pick a real entry from /sys/module and confirm it is reported as loaded
   DIR * d = opendir("/sys/module");
   if (d) {
      char found[256] = "";
      struct dirent * ent;
      while ((ent = readdir(d)) != NULL) {
         if (ent->d_name[0] == '.') continue;
         g_strlcpy(found, ent->d_name, sizeof(found));
         break;
      }
      closedir(d);
      if (found[0]) {
         CK(is_module_loaded_using_sysfs(found) == true);
      }
      else {
         printf("NOTE  /sys/module empty; positive case skipped\n");
      }
   }
   else {
      printf("NOTE  /sys/module not available; positive case skipped\n");
   }

   printf("\n%s: %d checks, %d passed, %d failed\n",
          (failed == 0) ? "PASS" : "FAIL", total, total - failed, failed);
   return (failed == 0) ? 0 : 1;
}
