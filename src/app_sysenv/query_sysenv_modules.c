/** \f query_sysenv_modules.c
 *
 *  Module checks
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


/** \cond */
#include <config.h>

// #define _GNU_SOURCE 1       // for function group_member
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/utsname.h>

#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_util.h"
/** \endcond */

#include "base/core.h"
#include "base/linux_errno.h"

#include "query_sysenv_modules.h"



/* Checks if a module is built in to the kernel.
 *
 * Arguments:
 *   module_name    simple module name, as it appears in the file system, e.g. i2c-dev
 *
 * Returns:         true/false
 */
bool is_module_builtin(char * module_name) {
   bool debug = false;
   bool result = false;

   struct utsname utsbuf;
   int rc = uname(&utsbuf);
   assert(rc == 0);

   char modules_builtin_fn[100];
   snprintf(modules_builtin_fn, 100, "/lib/modules/%s/modules.builtin", utsbuf.release);

#ifdef OLD
   // TODO: replace shell command with API read and scan of file,
   //       can use code from query_sysenv_logs.c

   char cmdbuf[200];

   snprintf(cmdbuf, 200, "grep -H %s.ko %s", module_name, modules_builtin_fn);
   // DBGMSG("cmdbuf = |%s|", cmdbuf);

   GPtrArray * response = execute_shell_cmd_collect(cmdbuf);
   // internal rc =  0 if found, 256 if not found
   // returns 0 lines if not found
   // NULL response if command error

   // DBGMSG("execute_shell_cmd_collect() returned %d lines", response->len);
   // for (int ndx = 0; ndx < response->len; ndx++) {
   //    puts(g_ptr_array_index(response, ndx));
   // }

   result = (response && response->len > 0);
   g_ptr_array_free(response, true);
#endif

   // new way
   char ko_name[40];
   snprintf(ko_name, 40, "%s.ko", module_name);

   bool builtin2 = false;
   GPtrArray * lines = g_ptr_array_new_full(400, g_free);
   char * terms[2];
   terms[0] = ko_name;
   terms[1] = NULL;
   int unfiltered_ct = read_file_with_filter(lines, modules_builtin_fn, terms, false, 0);
   if (unfiltered_ct < 0) {
      int errsv = errno;
      fprintf(stderr, "Error reading file %s: %s\n", modules_builtin_fn, linux_errno_desc(errsv));
      fprintf(stderr, "Assuming module %s is not built in to kernsl\n", module_name);
   }
   else {
      // DBGMSG("lines->len=%d", lines->len);
      builtin2 = (lines->len == 1);
   }
   g_ptr_array_free(lines, true);
   // DBGMSG("builtin2=%s", bool_repr(builtin2));
   result = builtin2;

   DBGMSF(debug, "module_name = %s, returning %s", module_name, bool_repr(result));
   return result;
}


/* Checks if a loadable module exists
 *
 * Arguments:
 *   module_name    simple module name, as it appears in the file system, e.g. i2c-dev,
 *                  without .ko, .ko.xz
 *
 * Returns:         true/false
 */
bool is_module_loadable(char * module_name, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting. module_name=%s", module_name);

   bool result = false;

   struct utsname utsbuf;
   int rc = uname(&utsbuf);
   assert(rc == 0);

   char module_name_ko[100];
   g_snprintf(module_name_ko, 100, "%s.ko", module_name);

   char dirname[PATH_MAX];
   g_snprintf(dirname, PATH_MAX, "/lib/modules/%s/kernel/drivers/i2c", utsbuf.release);

   struct dirent *dent;
     DIR           *d;
     d = opendir(dirname);
     if (!d) {
        rpt_vstring(depth,"Unable to open directory %s: %s", dirname, strerror(errno));
     }
     else {
        while ((dent = readdir(d)) != NULL) {
           // DBGMSG("%s", dent->d_name);
           if (!streq(dent->d_name, ".") && !streq(dent->d_name, "..") ) {
              if (str_starts_with(dent->d_name, module_name_ko)) {
                 result = true;
                 break;
              }
           }
        }
        closedir(d);
     }

   DBGMSF(debug, "Done. Returning: %s", bool_repr(result));
   return result;
}


/** Checks if module i2c_dev is required and if so whether it is loaded.
 *  Reports the result.
 *
 *  \param  accum  collects environment information
 *  \param  depth  logical indentation depth
 *
 *  \remark
 *  Sets #accum->module_i2c_dev_needed
 *       #accum->module_i2c_dev_loaded
 *       #accum->loadable_i2c_dev_exists
 */
void check_i2c_dev_module(Env_Accumulator * accum, int depth) {
   int d0 = depth;
   int d1 = depth+1;
   rpt_vstring(d0,"Checking for module i2c_dev...");
   DDCA_Output_Level output_level = get_output_level();

   accum->module_i2c_dev_needed = true;
   accum->i2c_dev_loaded_or_builtin = false;

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

   bool module_required = !only_fglrx(accum->driver_list);
   if (!module_required) {
      rpt_nl();
      rpt_vstring(d0,"Using only proprietary fglrx driver. Module i2c_dev not required.");
      accum->module_i2c_dev_needed = false;
   }
   else if (!is_builtin) {
      if (bva_length(accum->dev_i2c_device_numbers) == 0 && !is_loaded ) {
         rpt_nl();
         rpt_vstring(d0, "No /dev/i2c-N devices found, and module i2c_dev is not loaded.");
         rpt_nl();
      }
      if ( !is_loaded  || output_level >= DDCA_OL_VERBOSE) {
         rpt_nl();
         rpt_vstring(0,"Check that kernel module i2c_dev is being loaded by examining files where this would be specified...");
         execute_shell_cmd_rpt("grep -H i2c[-_]dev "
                           "/etc/modules "
                           "/etc/modules-load.d/*conf "
                           "/run/modules-load.d/*conf "
                           "/usr/lib/modules-load.d/*conf "
                           , d1);
         rpt_nl();
         rpt_vstring(0,"Check for any references to i2c_dev in /etc/modprobe.d ...");
         execute_shell_cmd_rpt("grep -H i2c[-_]dev "
                           "/etc/modprobe.d/*conf "
                           "/run/modprobe.d/*conf "
                           , d1);
      }
   }
}


void probe_modules_d(int depth) {
   rpt_nl();
   rpt_vstring(depth, "Video related contents of /etc/modprobe.d");
   char ** strings = get_all_driver_module_strings();
   char * grep_terms = strjoin((const char **) strings, ntsa_length(strings), "|");
   int bufsz = strlen(grep_terms) + 100;
   char * cmd = calloc(1, bufsz);
   // g_snprintf(cmd, bufsz, "grep -El \"\(%s)\" /etc/modprobe.d/*conf | xargs tail -n +1", grep_terms );
   g_snprintf(cmd, bufsz, "grep -EH \"\(%s)\" /etc/modprobe.d/*conf", grep_terms );
   // DBGMSG("cmd: %s", cmd);
   execute_shell_cmd_rpt(cmd, depth+1);
   free(cmd);
}
