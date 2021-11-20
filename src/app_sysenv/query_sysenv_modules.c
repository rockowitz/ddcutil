/** \f query_sysenv_modules.c
 *
 *  Module checks
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <config.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/utsname.h>

#include "util/linux_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_i2c_util.h"
#include "util/sysfs_util.h"
/** \endcond */

#include "base/core.h"
#include "base/linux_errno.h"

#include "query_sysenv_modules.h"


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
   // bool debug = false;
   int d0 = depth;
   int d1 = depth+1;
   rpt_vstring(d0,"Checking for driver i2c_dev...");
   DDCA_Output_Level output_level = get_output_level();

   // Eventually use only one test
   bool is_loaded = is_module_loaded_using_sysfs("i2c-dev");
   rpt_vstring(d1, "sysfs reports module i2c_dev is%s loaded.", (is_loaded) ? "" : " NOT");

   bool is_loaded2 = is_module_loaded_using_libkmod("i2c-dev");
   if (is_loaded2 != is_loaded) {
      rpt_vstring(d1, "BUT libkmod reports module i2c_dev is%s loaded. !!!", (is_loaded) ? "" : " NOT");
      rpt_vstring(d1, "REGARDING sysfs AS CORRECT !!!");
   }


   bool is_builtin = false;
   bool loadable = false;
   int module_status = module_status_using_libkmod("i2c-dev");
   if (module_status < 0) {
      rpt_vstring(d1, "Unable to determine i2c-dev status.");
      rpt_vstring(d1, "module_status_using_libkmod() returned %s", psc_desc(module_status));
      rpt_vstring(d1, "Treating i2c-dev as not builtin and not loadable!!!");
   }
   else if (module_status == 0) {
      rpt_vstring(d1, "Kernel module i2c-dev does not exist!");
   }
   if (module_status == KERNEL_MODULE_BUILTIN)
      is_builtin = true;
   if (module_status == KERNEL_MODULE_LOADABLE_FILE)
      loadable = true;

   accum->module_i2c_dev_needed = true;           // relic from driver fglrx, which did not require dev-i2c
   accum->module_i2c_dev_builtin = is_builtin;
   accum->loadable_i2c_dev_exists = loadable;
   accum->i2c_dev_loaded_or_builtin = is_loaded || is_builtin;

   rpt_vstring(d1, "Module i2c_dev is%s built into the kernel",
                      (is_builtin) ? "" : " NOT");
   if (!is_builtin) {
      rpt_vstring(d1,"Loadable i2c-dev module %sfound", (accum->loadable_i2c_dev_exists) ? "" : "NOT ");
      rpt_vstring(d1,"Module %s is %sloaded", "i2c_dev", (is_loaded) ? "" : "NOT ");

      assert(accum->dev_i2c_device_numbers);    // already set
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


/** Reports video related contents of directory /etc/modprobe.d
 *
 *  @param depth  logical indentation depth
 */
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
   free(grep_terms);
   free(cmd);
}
