/** \file linux_util.c
 * Miscellaneous Linux utilities
 */

// Copyright (C) 2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/utsname.h>

#include "file_util.h"
#include "string_util.h"

/** Checks if a module is built into the kernel.
  *
  * \param  module_name  simple module name, as it appears in the file system, e.g. i2c-dev
  * \return true/false
  */
bool is_module_builtin(char * module_name)
{
   bool debug = false;
   bool result = false;

   struct utsname utsbuf;
   int rc = uname(&utsbuf);
   assert(rc == 0);

   char modules_builtin_fn[100];
   snprintf(modules_builtin_fn, 100, "/lib/modules/%s/modules.builtin", utsbuf.release);

   char ko_name[40];
   snprintf(ko_name, 40, "%s.ko", module_name);

   result = false;
   GPtrArray * lines = g_ptr_array_new_full(400, g_free);
   char * terms[2];
   terms[0] = ko_name;
   terms[1] = NULL;
   int unfiltered_ct = read_file_with_filter(lines, modules_builtin_fn, terms, false, 0);
   if (unfiltered_ct < 0) {
      char buf[100];
      strerror_r(errno, buf, 100);
      fprintf(stderr, "Error reading file %s: %s\n", modules_builtin_fn, buf);
      fprintf(stderr, "Assuming module %s is not built in to kernel\n", module_name);
   }
   else {
      result = (lines->len == 1);
   }
   g_ptr_array_free(lines, true);
   if (debug)
      printf("(%s) module_name=%s, returning %s\n",__func__,  module_name, sbool(result));
   return result;
}

#ifdef REF
 bool is_builtin = is_module_builtin("i2c-dev");
   accum->module_i2c_dev_builtin = is_builtin;
   rpt_vstring(d1,"Module %s is %sbuilt into kernel", "i2c-dev", (is_builtin) ? "" : "NOT ");

   accum->loadable_i2c_dev_exists = is_module_loadable("i2c-dev", d1);
   if (!is_builtin)
      rpt_vstring(d1,"Loadable i2c-dev module %sfound", (accum->loadable_i2c_dev_exists) ? "" : "NOT ");

   bool is_loaded = is_module_loaded_using_sysfs("i2c_dev");
   accum->i2c_dev_loaded_or_builtin = is_loaded || is_builtin;
   if (!is_builtin)
      rpt_vstring(d1,"Module %s is %sloaded", "i2c_dev", (is_loaded) ? "" : "NOT ");
#endif
