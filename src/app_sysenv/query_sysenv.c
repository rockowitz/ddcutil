/* query_sysenv.c
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#include <config.h>

#define _GNU_SOURCE 1       // for function group_member

/** \cond */
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>         // glib-2.0/ to avoid bogus eclipse error
#include <grp.h>
#include <limits.h>
// #include <libosinfo-1.0/osinfo/osinfo.h>
// #include <libudev.h>        // not yet used
#include <linux/hiddev.h>
#include <linux/limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#ifdef USE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#endif

#include "util/data_structures.h"
#include "util/device_id_util.h"
#include "util/edid.h"
#include "util/file_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_util.h"
#ifdef PROBE_USING_SYSTEMD
#include "util/systemd_util.h"
#endif
#ifdef USE_X11
#include "util/x11_util.h"
#endif
#include "util/udev_i2c_util.h"
#include "util/udev_usb_util.h"
#include "util/udev_util.h"
/** \endcond */

#include "base/build_info.h"
#include "base/core.h"
#include "base/ddc_errno.h"
#include "base/linux_errno.h"

#include "i2c/i2c_bus_core.h"

#include "ddc/ddc_packet_io.h"

#include "adl/adl_shim.h"

#include "query_sysenv_access.h"
#include "query_sysenv_base.h"
#include "query_sysenv_dmidecode.h"
#include "query_sysenv_drm.h"
#include "query_sysenv_i2c.h"
#include "query_sysenv_logs.h"
#include "query_sysenv_procfs.h"
#include "query_sysenv_sysfs.h"
#include "query_sysenv_xref.h"

#include "query_sysenv.h"



/** Compile time and runtime checks of endianness.
 *
 *  \param depth logical indentation depth
 */
static void report_endian(int depth) {
   int d1 = depth+1;
   rpt_title("Byte order checks:", depth);

   bool is_bigendian = (*(uint16_t *)"\0\xff" < 0x100);
   rpt_vstring(d1, "Is big endian (local test):       %s", bool_repr(is_bigendian));

   rpt_vstring(d1, "WORDS_BIGENDIAN macro (autoconf): "
#ifdef WORDS_BIGENDIAN
         "defined"
#else
         "not defined"
#endif
         );
   rpt_vstring(d1, "__BYTE_ORDER__ macro (gcc):       "
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
         "__ORDER_LITTLE_ENDIAN__"
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
         "__ORDER_BIG_ENDIAN__"
#elif __BYTE_ORDER__ == __ORDER_PDP_ENDIAN__
         "__ORDER_PDP_ENDIAN__"
#else
         "unexpected value"
#endif
         );

#ifdef REDUNDANT
   __u32 i = 1;
   bool is_bigendian2 =  ( (*(char*)&i) == 0 );
   rpt_vstring(d1, "Is big endian (runtime test): %s", bool_repr(is_bigendian2));
#endif
}


//
// Higher level functions
//

/** Reports basic system information
 *
 * \param  accum  pointer to struct in which information is returned
 */
static void query_base_env(Env_Accumulator * accum) {
   int d0 = 0;
   int d1 = d0+1;

   rpt_vstring(d0, "ddcutil version: %s", BUILD_VERSION);
   rpt_nl();

   sysenv_rpt_file_first_line("/proc/version", NULL, 0);

   char * expected_architectures[] = {"x86_64", "i386", "i686", "armv7l", "aarch64", "ppc64",  NULL};
   // n. alternative command "arch" not found on Arch Linux
   // uname -m  machine hardware name
   // uname -p  processor type (non-portable)
   // uname -i  hardware platform (non-portable)
   accum->architecture   = execute_shell_cmd_one_line_result("uname -m");
   accum->distributor_id = execute_shell_cmd_one_line_result("lsb_release -s -i");  // e.g. Ubuntu, Raspbian
   char * release        = execute_shell_cmd_one_line_result("lsb_release -s -r");
   rpt_nl();
   rpt_vstring(d0, "Architecture:     %s", accum->architecture);
   rpt_vstring(d0, "Distributor id:   %s", accum->distributor_id);
   rpt_vstring(d0, "Release:          %s", release);

   if ( ntsa_find(expected_architectures, accum->architecture) >= 0) {
      rpt_vstring(d0, "Found a known architecture");
   }
   else {
      rpt_vstring(d0, "Unexpected architecture %s.  Please report.", accum->architecture);
   }

   accum->is_raspbian = accum->distributor_id && streq(accum->distributor_id, "Raspbian");
   accum->is_arm      = accum->architecture   &&
                        ( str_starts_with(accum->architecture, "arm") ||
                          str_starts_with(accum->architecture, "aarch")
                        );
   free(release);

#ifdef REDUNDANT
   rpt_nl();
   rpt_vstring(0,"/etc/os-release...");
   bool ok = execute_shell_cmd_rpt("grep PRETTY_NAME /etc/os-release", 1 /* depth */);
   if (!ok)
      rpt_vstring(1,"Unable to read PRETTY_NAME from /etc/os-release");
#endif

   rpt_nl();
   sysenv_rpt_file_first_line("/proc/cmdline", NULL, d0);

   if (get_output_level() >= DDCA_OL_VERBOSE) {
      rpt_nl();
      rpt_vstring(d0,"Processor information as reported by lscpu:");
        bool ok = execute_shell_cmd_rpt("lscpu", 1);
        if (!ok) {   // lscpu should always be there, but just in case:
           rpt_vstring(1, "Command lscpu not found");
           rpt_nl();
           rpt_title("Processor information from /proc/cpuinfo:", d0);
           // uniq because entries for each processor of a mulit-processor cpu
           execute_shell_cmd_rpt( "cat /proc/cpuinfo | grep vendor_id | uniq", d1);
           execute_shell_cmd_rpt( "cat /proc/cpuinfo | grep \"cpu family\" | uniq", d1);
           execute_shell_cmd_rpt( "cat /proc/cpuinfo | grep \"model[[:space:]][[:space:]]\" | uniq",  d1);   //  "model"
           execute_shell_cmd_rpt( "cat /proc/cpuinfo | grep \"model name\" | uniq",  d1);   // "model name"
       }

       rpt_nl();
        if (accum->is_arm) {
           rpt_vstring(d0, "Skipping dmidecode checks on architecture %s.", accum->architecture);
        }
        else {
           query_dmidecode();
        }

      rpt_nl();
      report_endian(d0);
   }

}


/* Checks if a module is built in to the kernel.
 *
 * Arguments:
 *   module_name    simple module name, as it appears in the file system, e.g. i2c-dev
 *
 * Returns:         true/false
 */
static bool is_module_builtin(char * module_name) {
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
      fprintf(FERR, "Error reading file %s: %s\n", modules_builtin_fn, linux_errno_desc(errsv));
      fprintf(FERR, "Assuming module %s is not built in to kernsl\n", module_name);
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
static bool is_module_loadable(char * module_name, int depth) {
   bool debug = false;
   DBGMSF("Starting. module_name=%s", module_name);

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


/* Checks if module i2c_dev is required and if so whether it is loaded.
 * Reports the result.
 *
 * \param  accum  collects environment information
 * \param  depth  logical indentation depth
 *
 * \remark
 * Sets #accum->module_i2c_dev_needed
 *      #accum->module_i2c_dev_loaded
 *      #accum->loadable_i2c_dev_exists
 */
static void check_i2c_dev_module(Env_Accumulator * accum, int depth) {
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

   bool module_required = !only_nvidia_or_fglrx(accum->driver_list);
   if (!module_required) {
      rpt_nl();
      rpt_vstring(d0,"Using only proprietary nvidia or fglrx driver. Module i2c_dev not required.");
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


#ifdef OLD

/* Checks for installed packages i2c-tools and libi2c-dev
 */
static void query_packages() {
   rpt_multiline(0,
         "ddcutil requiries package i2c-tools.  Use both dpkg and rpm to look for it.",
          "While we're at it, check for package libi2c-dev which is used for building",
          "ddcutil.",
          NULL
         );

   bool ok;
   // n. apt show produces warning msg that format of output may change.
   // better to use dpkg
   rpt_nl();
   rpt_vstring(0,"Using dpkg to look for package i2c-tools...");
   ok = execute_shell_cmd_rpt("dpkg --status i2c-tools", 1);
   if (!ok)
      rpt_vstring(0,"dpkg command not found");
   else {
      execute_shell_cmd_rpt("dpkg --listfiles i2c-tools", 1);
   }

   rpt_nl();
   rpt_vstring(0,"Using dpkg to look for package libi2c-dev...");
   ok = execute_shell_cmd_rpt("dpkg --status libi2c-dev", 1);
   if (!ok)
      rpt_vstring(0,"dpkg command not found");
   else {
      execute_shell_cmd_rpt("dpkg --listfiles libi2c-dev", 1);
   }

   rpt_nl();
   rpt_vstring(0,"Using rpm to look for package i2c-tools...");
   ok = execute_shell_cmd_rpt("rpm -q -l --scripts i2c-tools", 1);
   if (!ok)
      rpt_vstring(0,"rpm command not found");
}
#endif


/* Performs checks specific to the nvidia and fglrx proprietary video drivers.
 *
 * Arguments:
 *    driver list    list of loaded drivers
 *
 * Returns:          nothing
 */
static void driver_specific_tests(struct driver_name_node * driver_list) {
   rpt_vstring(0,"Performing driver specific checks...");
   bool found_driver_specific_checks = false;

   if (driver_name_list_find_prefix(driver_list, "nvidia")) {
      found_driver_specific_checks = true;
      rpt_nl();
      rpt_vstring(0,"Checking for special settings for proprietary Nvidia driver ");
      rpt_vstring(0,"(Needed for some newer Nvidia cards).");
      execute_shell_cmd_rpt("grep -iH i2c /etc/X11/xorg.conf /etc/X11/xorg.conf.d/*", 1);
   }

   if (driver_name_list_find_prefix(driver_list, "fglrx")) {
      found_driver_specific_checks = true;
      rpt_nl();
      rpt_vstring(0,"Performing ADL specific checks...");
#ifdef HAVE_ADL
     if (!adlshim_is_available()) {
        set_output_level(DDCA_OL_VERBOSE);  // force error msg that names missing dll
        bool ok = adlshim_initialize();
        if (!ok)
           printf("WARNING: Using AMD proprietary video driver fglrx but unable to load ADL library\n");
     }
#else
     rpt_vstring(0,"WARNING: Using AMD proprietary video driver fglrx but ddcutil built without ADL support");
#endif
   }

   if (!found_driver_specific_checks)
      rpt_vstring(0,"No driver specific checks apply.");
}


#ifdef USE_X11
//
// Using X11 API
//

/* Reports EDIDs known to X11
 *
 * Arguments:    none
 *
 * Returns:      nothing
 */
void query_x11() {
   GPtrArray* edid_recs = get_x11_edids();
   rpt_nl();
   rpt_vstring(0,"EDIDs reported by X11 for connected xrandr outputs:");
   // DBGMSG("Got %d X11_Edid_Recs\n", edid_recs->len);

   for (int ndx=0; ndx < edid_recs->len; ndx++) {
      X11_Edid_Rec * prec = g_ptr_array_index(edid_recs, ndx);
      // printf(" Output name: %s -> %p\n", prec->output_name, prec->edid);
      // hex_dump(prec->edid, 128);
      rpt_vstring(1, "xrandr output: %s", prec->output_name);
      rpt_label  (2, "Raw EDID:");
      rpt_hex_dump(prec->edidbytes, 128, 2);
      Parsed_Edid * parsed_edid = create_parsed_edid(prec->edidbytes);
      if (parsed_edid) {
         report_parsed_edid_base(
               parsed_edid,
               true,   // verbose
               false,  // show_hex
               2);     // depth
         free_parsed_edid(parsed_edid);
      }
      else {
         rpt_label(2, "Unable to parse EDID");
         // printf(" Unparsable EDID for output name: %s -> %p\n", prec->output_name, prec->edidbytes);
         // hex_dump(prec->edidbytes, 128);
      }
      rpt_nl();

      Device_Id_Xref * xref = device_xref_get(prec->edidbytes);
      xref->xrandr_name = strdup(prec->output_name);
   }
   free_x11_edids(edid_recs);

   // Display * x11_disp = open_default_x11_display();
   // GPtrArray *  outputs = get_x11_connected_outputs(x11_disp);
   // close_x11_display(x11_disp);
}
#endif


//
// i2cdetect
//

/** Examines /dev/i2c devices using command i2cdetect, if it exists.
 *
 *  \param  i2c_device_numbers  I2C bus numbers to check
 *
 */
static void query_using_i2cdetect(Byte_Value_Array i2c_device_numbers) {
   assert(i2c_device_numbers);

   int d0 = 0;
   int d1 = 1;

   rpt_vstring(d0,"Examining I2C buses using i2cdetect... ");

   if (bva_length(i2c_device_numbers) == 0) {
      rpt_vstring(d1, "No I2C buses found");
   }
   else {
      for (int ndx=0; ndx< bva_length(i2c_device_numbers); ndx++) {
         int busno = bva_get(i2c_device_numbers, ndx);
         if (is_ignorable_i2c_device(busno)) {
            // calling i2cdetect for an SMBUs device fills dmesg with error messages
            rpt_nl();
            rpt_vstring(d1, "Device /dev/i2c-%d is a SMBus or other ignorable device.  Skipping i2cdetect.", busno);
         }
         else {
            char cmd[80];
            snprintf(cmd, 80, "i2cdetect -y %d", busno);
            rpt_nl();
            rpt_vstring(d1,"Probing bus /dev/i2c-%d using command \"%s\"", busno, cmd);
            int rc = execute_shell_cmd_rpt(cmd, 2 /* depth */);
            // DBGMSG("execute_shell_cmd(\"%s\") returned %d", cmd, rc);
            if (rc != 1) {
               rpt_vstring(d1,"i2cdetect command unavailable");
               break;
            }
         }
      }
   }
}


/** Queries UDEV for devices in subsystem "i2c-dev".
 *  Also looks for devices with name attribute "DPMST"
 */
static void probe_i2c_devices_using_udev() {
   char * subsys_name = "i2c-dev";
   rpt_vstring(0,"Probing I2C devices using udev, susbsystem %s...", subsys_name);
   // probe_udev_subsystem() is in udev_util.c, which is only linked in if USE_USB

   // Detailed scan of I2C device information
   probe_udev_subsystem(subsys_name, /*show_usb_parent=*/ false, 1);
   rpt_nl();

   GPtrArray * summaries = get_i2c_devices_using_udev();
   report_i2c_udev_device_summaries(summaries, "Summary of udev I2C devices",1);
   for (int ndx = 0; ndx < summaries->len; ndx++) {
      Udev_Device_Summary * summary = g_ptr_array_index(summaries, ndx);
      assert( memcmp(summary->marker, UDEV_DEVICE_SUMMARY_MARKER, 4) == 0);
      int busno = udev_i2c_device_summary_busno(summary);
      Device_Id_Xref * xref = device_xref_find_by_busno(busno);
      if (xref) {
         xref->udev_name = strdup(summary->sysattr_name);
         xref->udev_syspath = strdup(summary->devpath);
      }
      else {
         // DBGMSG("Device_Id_Xref not found for busno %d", busno);
      }
   }
   free_udev_device_summaries(summaries);   // ok if summaries == NULL

   rpt_nl();
   char * nameattr = "DPMST";
   rpt_vstring(0,"Looking for udev devices with name attribute %s...", nameattr);
   summaries = find_devices_by_sysattr_name(nameattr);
   report_i2c_udev_device_summaries(summaries, "Summary of udev DPMST devices...",1);
   free_udev_device_summaries(summaries);   // ok if summaries == NULL
}


/** Analyze collected environment information, Make suggestions.
 *
 * \param accum  accumulated environment information
 * \param depth  logical indentation depth
 */
void final_analysis(Env_Accumulator * accum, int depth) {
   int d1 = depth + 1;
   int d2 = depth + 2;
   int d3 = depth + 3;

   // for testing:
   // accum->dev_i2c_common_group_name = NULL;
   // accum->module_i2c_dev_loaded_or_builtin = false;
   // accum->cur_user_all_devi2c_rw = false;
   // accum->all_dev_i2c_is_group_rw = false;

   bool odd_groups = accum->dev_i2c_common_group_name &&
                    !streq(accum->dev_i2c_common_group_name, "root") &&
                    !streq(accum->dev_i2c_common_group_name, "i2c");

   bool msg_issued = false;
   rpt_vstring(depth, "Configuration suggestions:");

   if (odd_groups) {
      rpt_label  (d1, "Issue:");
      rpt_label  (d2, "/dev/i2c-N devices have non-standard or varying group names.");
      rpt_label  (d2, "Suggestions are incomplete.");
      rpt_nl();
      msg_issued = true;
   }

   // TODO: Also compare dev_i2c_devices vs sys_bus_i2c_devices ?
   if (bva_length(accum->dev_i2c_device_numbers) == 0 &&
       accum->module_i2c_dev_needed &&
       !accum->i2c_dev_loaded_or_builtin)
   {
      rpt_label  (d1, "Issue:");
      rpt_label  (d2, "No /dev/i2c-N devices found.");
      rpt_vstring(d2, "%sI2C devices exist in /sys/bus/i2c", (accum->sysfs_i2c_devices_exist) ? "" : "No ");
      rpt_label  (d2, "Module dev-i2c is required.");
      rpt_label  (d2, "Module dev-i2c is not loaded");
      rpt_label  (d1, "Suggestion:");
      rpt_label  (d2, "Manually load module i2c-dev using the command:");
      rpt_label  (d3, "sudo modprobe i2c-dev");
      rpt_label  (d2,  "If this solves the problem, put an entry in directory /etc/modules-load.d");
      rpt_label  (d2, "that will cause i2c-dev to be loaded.  Type \"man modules-load.d\" for details");
      rpt_nl();
      msg_issued = true;
   }

   else {
      if (accum->cur_user_all_devi2c_rw) {   // n. will be true if no /dev/i2c-N devices exist
         rpt_label(d1, "Current user has RW access to all /dev/i2c-N devices.");
         rpt_label(d1, "Skipping further group and permission checks.");
         rpt_nl();
         msg_issued = true;
      }
      else {
         if (accum->cur_user_any_devi2c_rw) {
            rpt_label  (d1,   "Issue:");
            rpt_multiline(d2, "Current user has RW access to some but not all /dev/i2c-N devices.",
                              "If there is RW access to at least the /dev/i2c-N devices for connected monitors,",
                              "this is not a problem.",
                              "Remaining suggestions assume RW access is still to be established.",
                              "",
                              NULL
                             );
            msg_issued = true;
         }
         if (!accum->group_i2c_exists) {
            rpt_label  (d1, "Issue:");
            rpt_label  (d2, "Group i2c does exist.");
            rpt_label  (d1, "Suggestion:");
            rpt_label  (d2, "Create group i2c. To create group i2c, use command:");
            rpt_label  (d3, "sudo groupadd --system i2c");
            rpt_label  (d2, "Assign /dev/i2c-N devices to group i2c by adding a rule to /etc/udev/rules.d");
            rpt_label  (d2, "Add the current user to group i2c:");
            rpt_label  (d3,  "sudo usermod -G i2c -a <username>");
            rpt_label  (d2, "After this, you will have to logout and login again.");
            rpt_label  (d2, "The changes to the user's group list are not read until a new login.");
            rpt_nl();
            msg_issued = true;
         }
         else {  // group i2c exists
            if (!accum->all_dev_i2c_has_group_i2c) {
               // Punt on odd case were some but not all /dev/i2c-N devices are in group i2c
               if (accum->any_dev_i2c_has_group_i2c) {
                  rpt_label  (d1,   "Issue:");
                  rpt_multiline(d2, "Some but not all /dev/i2c-N devices have group i2c.",
                                    "If the /dev/i2c-N devices for connected monitors have group i2c,"
                                    "this is not a problem. Remaining suggestions assume /dev/i2c-N "
                                    "devices require assignment to group i2c.",
                                    "",
                                    NULL
                                   );
                  msg_issued = true;
               }

               rpt_label  (d1, "Issue:");
               rpt_label  (d2, "/dev/i2c-N devices not assigned to group i2c");
               rpt_label  (d1, "Suggestion:");
               rpt_label  (d2, "Assign /dev/i2c-N devices to group i2c by adding or editing a rule");
               rpt_label  (d2, "in /etc/udev/rules.d");
               rpt_nl();
               msg_issued = true;
            }
            // handle case of /dev/i2c-N devices have group i2c, but not RW

            if (!accum->cur_user_in_group_i2c) {
               rpt_label  (d1, "Issue:");
               rpt_label  (d2, "Current user is not a member of group i2c");
               rpt_label  (d1, "Suggestion:");
               rpt_label  (d2, "Execute command:");
               rpt_vstring(d3, "sudo usermod -G i2c -a %s", accum->cur_uname);
               rpt_label  (d2, "After this, you will have to logout and login again.");
               rpt_label  (d2, "The changes to the user's group list are not read until a new login.");
               rpt_nl();
               msg_issued = true;
            }
         }

         if (!accum->all_dev_i2c_is_group_rw && !streq(accum->dev_i2c_common_group_name, "root")) {
            rpt_label  (d1, "Issue:");
            rpt_label  (d2, "At least some /dev/i2c-N devices do not have group RW permission.");
            rpt_label  (d1, "Suggestion:");
            rpt_label  (d2, "Set group RW access to /dev/i2c-N devices by adding or editing a rule");
            rpt_label  (d2, "in /etc/udev/rules.d");
            rpt_nl();
            msg_issued = true;
         }
      }
   }

   if (!msg_issued) {
      rpt_vstring(d1, "None");
      rpt_nl();
   }
}

//
// Mainline
//

/* Master function to query the system environment
 *
 * Arguments:    none
 *
 * Returns:      nothing
 */
void query_sysenv() {
   device_xref_init();

   Env_Accumulator * accumulator = env_accumulator_new();

   rpt_nl();
   rpt_vstring(0,"*** Basic System Information ***");
   rpt_nl();
   query_base_env(accumulator);

   rpt_nl();
   rpt_vstring(0,"*** Primary Check 1: Identify video card and driver ***");
   rpt_nl();
   query_card_and_driver_using_sysfs(accumulator);


   rpt_nl();
   rpt_vstring(0,"*** Primary Check 2: Check that /dev/i2c-* exist and writable ***");
   rpt_nl();
   accumulator->dev_i2c_device_numbers = identify_i2c_devices();
   assert(accumulator->dev_i2c_device_numbers);
   // redundant
   // rpt_vstring(0, "Identified %d I2C devices", bva_length(accumulator->dev_i2c_device_numbers));
   // rpt_nl();
   check_i2c_devices(accumulator);

   rpt_nl();
   rpt_vstring(0,"*** Primary Check 3: Check that module i2c_dev is loaded ***");
   rpt_nl();
   check_i2c_dev_module(accumulator, 0);

   rpt_nl();
   rpt_vstring(0,"*** Primary Check 4: Driver specific checks ***");
   rpt_nl();
   driver_specific_tests(accumulator->driver_list);

   // TODO: move to end of function
   // Free the driver list created by query_card_and_driver_using_sysfs()
   // free_driver_name_list(accumulator->driver_list);
   // driver_list = NULL;

#ifdef OLD
   rpt_nl();
   rpt_vstring(0,"*** Primary Check 5: Installed packages ***");
   rpt_nl();
   query_packages();
   rpt_nl();
#endif

   rpt_nl();
   rpt_vstring(0,"*** Additional probes ***");
   // printf("Gathering card and driver information...\n");
   rpt_nl();
   query_proc_modules_for_video();
   if (!accumulator->is_arm) {
      // rpt_nl();
      // query_card_and_driver_using_lspci();
      //rpt_nl();
      //query_card_and_driver_using_lspci_alt();
   }
   rpt_nl();
   query_loaded_modules_using_sysfs();
   rpt_nl();
   query_sys_bus_i2c(accumulator);

   DDCA_Output_Level output_level = get_output_level();
   if (output_level >= DDCA_OL_VERBOSE) {
      rpt_nl();
      query_proc_driver_nvidia();

      rpt_nl();
      query_i2c_buses();

      rpt_nl();
      rpt_vstring(0,"xrandr connection report:");
      execute_shell_cmd_rpt("xrandr|grep connected", 1 /* depth */);
      rpt_nl();

      rpt_vstring(0,"Checking for possibly conflicting programs...");
      execute_shell_cmd_rpt("ps aux | grep ddccontrol | grep -v grep", 1);
      rpt_nl();

      query_using_i2cdetect(accumulator->dev_i2c_device_numbers);

      raw_scan_i2c_devices(accumulator);

#ifdef USE_X11
      query_x11();
#endif

      // probe_udev_subsystem() is in udev_util.c, which is only linked in if USE_USB
      probe_i2c_devices_using_udev();

      // temp
      // get_i2c_smbus_devices_using_udev();

      probe_config_files(accumulator);
      probe_logs(accumulator);

#ifdef USE_LIBDRM
      probe_using_libdrm();
#else
      rpt_vstring(0, "Not built with libdrm support.  Skipping DRM related checks");
#endif

      query_drm_using_sysfs();

      device_xref_report(0);
   }

   rpt_nl();
   env_accumulator_report(accumulator, 0);

   rpt_nl();
   final_analysis(accumulator, 0);

   env_accumulator_free(accumulator);     // make Coverity happy
}

