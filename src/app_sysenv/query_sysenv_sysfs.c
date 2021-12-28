/** @file query_sysenv_sysfs.c
 *
 *  Query environment using /sys file system
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>

#include "util/data_structures.h"
#include "util/device_id_util.h"
#include "util/file_util.h"
#include "util/i2c_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_filter_functions.h"
#include "util/sysfs_i2c_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"
#include "base/linux_errno.h"
#include "base/rtti.h"
/** \endcond */

#include "i2c/i2c_sysfs.h"

#include "query_sysenv_base.h"
#include "query_sysenv_sysfs_common.h"
#include "query_sysenv_original_sys_scans.h"
#include "query_sysenv_detailed_bus_pci_devices.h"
#include "query_sysenv_simplified_sys_bus_pci_devices.h"
#include "query_sysenv_xref.h"

#include "query_sysenv_sysfs.h"

// Default trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_ENV;

// Notes on directory structure
//
// Raspbian:
// /sys/bus/platform/drivers/vc4_v3d
// /sys/module/vc4


// Two ways to get the hex device identifiers.  Both are ugly.
// Reading modalias requires extracting values from a single string.
// Reading individual ids from individual attributes is simpler,
// but note the lack of error checking.
// Pick your poison.

/** Reads the device identifiers from directory
 *  /sys/bus/pci/devices/nnnn:nn:nn.n/ using the individual vendor, device,
 *  subsystem, and subsystem_device attributes.
 *
 *  \param cur_dir_name  directory name
 *  \return struct containing the ids
 *
 *  \remark
 *  Note the struct itself is returned on the stack, not a pointer to a struct.
 *  There is nothing to free.
 */
Device_Ids read_device_ids1(char * cur_dir_name) {
   Device_Ids result = {0};

   char * vendor_id        = read_sysfs_attr_w_default(cur_dir_name, "vendor",           "0x00", true);
   char * device_id        = read_sysfs_attr_w_default(cur_dir_name, "device",           "0x00", true);
   char * subsystem_device = read_sysfs_attr_w_default(cur_dir_name, "subsystem_device", "0x00", true);
   char * subsystem_vendor = read_sysfs_attr_w_default(cur_dir_name, "subsystem_vendor", "0x00", true);

   result.vendor_id    = h2ushort(vendor_id);
   result.device_id    = h2ushort(device_id);
   result.subvendor_id = h2ushort(subsystem_vendor);
   result.subdevice_id = h2ushort(subsystem_device);

   free(vendor_id);
   free(device_id);
   free(subsystem_device);
   free(subsystem_vendor);

   return result;
}


/** Reads the device identifiers from the directory of a PCI device
 *  (/sys/bus/pci/devices/nnnn:nn:nn.n/) by reading and parsing the modalias
 *  attribute.
 *
 *  \param cur_dir_name  directory name
 *  \return struct containing the ids
 *
 *  \remark
 *  Note the struct itself is returned on the stack, not a pointer to a struct.
 *  There is nothing to free.
 */
Device_Ids read_device_ids2(char * cur_dir_name) {
   Device_Ids result = {0};

   // TODO: Reimplement using proper parsing.  See kernel file file2alias.c
   // See also:
   //  http://people.skolelinux.org/pere/blog/Modalias_strings___a_practical_way_to_map__stuff__to_hardware.html

   char * modalias = read_sysfs_attr(cur_dir_name, "modalias", true);
               // printf("modalias: %s\n", modalias);
   if (modalias) {
      // printf("\nParsing modalias for values...\n");
      char * colonpos = strchr(modalias, ':');
      assert(colonpos);                // coverity complains that strchr() might return NULL
      assert(*(colonpos+1) == 'v');    // vendor_id
      char * vendor_id = substr(colonpos, 2, 8);
      // printf("vendor_id:        %s\n", vendor_id);
      assert(*(colonpos+10) == 'd');
      char * device_id = lsub(colonpos+11,8);
      // printf("device_id:        %s\n", device_id);
      assert( *(colonpos+19) == 's');
      assert( *(colonpos+20) == 'v');
      char * subsystem_vendor = lsub(colonpos+21,8);
      // printf("subsystem_vendor: %s\n", subsystem_vendor);
      assert( *(colonpos+29) == 's');
      assert( *(colonpos+30) == 'd');
      char * subsystem_device = lsub(colonpos+31,8);
      // printf("subsystem_device: %s\n", subsystem_device);
      assert( *(colonpos+39) == 'b');
      assert( *(colonpos+40) == 'c');
      // not used
      //char * base_class = lsub(colonpos+41,2);
      // printf("base_class:       %s\n", base_class);     // bytes 0-1 of value from class
      assert( *(colonpos+43) == 's');
      assert( *(colonpos+44) == 'c');
      // not used
      // char * sub_class = lsub(colonpos+45,2);          // bytes 1-2 of value from class
      // printf("sub_class:        %s\n", sub_class);
      assert( *(colonpos+47) == 'i');
      // not used
      // char * interface_id = lsub(colonpos+48,2);
      // printf("interface_id:     %s\n", interface_id);  // bytes 4-5 of value from class?

      result.vendor_id    = h2ushort(vendor_id);
      result.device_id    = h2ushort(device_id);
      result.subvendor_id = h2ushort(subsystem_vendor);
      result.subdevice_id = h2ushort(subsystem_device);

      free(vendor_id);
      free(device_id);
      free(subsystem_vendor);
      free(subsystem_device);
      free(modalias);
   }

   return result;
}

#ifdef FUTURE_FRAGMENT

void report_modalias(char * cur_dir_name, int depth) {
   char * modalias = read_sysfs_attr(cur_dir_name, "modalias", true);
               // printf("modalias: %s\n", modalias);
   if (modalias) {
      char * colonpos = strchr(modalias, ':');
      assert(colonpos);                // coverity complains that strchr() might return NULL
      if (!colonpos) {
         rpt_vstring(depth, "Unexpected modalias value: %s", modalias);
      }
      if (memcmp(modalias, "pci", (colonpos-1)-modalias) == 0) {
         // TO DO: properly refactor
         Device_Ids = read_device_ids2(cur_dir_name);
         // TOD: report it
      }
      else if (memcmp(modalias, "of", (colonpos-1)-modalias) == 0) {
         // format:  of:NnameTtypeCclass
         //     type may be "<NULL>"
         //      Cclass is optional
         //   may repeat?

         char * re = "^of:N(.*)T(.*)"

      }
      else {
         rpt_vstring(depth, "modialias: %s", modalias);
      }



   }
}
#endif


/** Reports one directory whose name is of the form /sys/bus/pci/devices/nnnn:nn:nn.n/driver
 *
 *  Processes only files whose name is of the form i2c-n,
 *  reporting the i2c-n dname and the the contained name attribute.
 *
 *  This function is passed from #each_video_pci_device()  to #dir_foreach() ,
 *  which in turn invokes this function.
 *
 *  \param dirname      always /sys/bus/pci/devices/nnnn:nn:nn.n/driver
 *  \param fn           fn, process only those of form i2c-n
 *  \param accumulator  accumulator struct
 *  \param depth        logical indentation depth
 */
void each_video_device_i2c(
      const char * dirname,
      const char * fn,
      void *       accumulator,
      int          depth)
{
   bool debug = false;
   DBGMSF(debug, "dirname=%s, fn=%s", dirname, fn);

   if (str_starts_with(fn, "i2c")) {
      char cur_dir[PATH_MAX];
      snprintf(cur_dir, PATH_MAX, "%s/%s", dirname, fn);
      char * name = read_sysfs_attr_w_default(cur_dir, "name","", false);
      rpt_vstring(depth, "I2C device:          %-10s name: %s", fn, name);
      free(name);
   }
}


/** Reports the device identifiers in directory /sys/bus/pci/devices/nnnn:nn:nn.n
 *
 *  Note that the devices/nnnn:nn:nn.n under /sys/bus/pci always has
 *  vendor/device etc from modalias extracted into individual attributes.
 *  Other device subdirectories do not necessarily have these attributes.
 *
 *  \param sysfs_device_dir   always /sys/bus/pci/devices/nnnn:nn:nn.n
 *  \param depth              logical indentation depth
 */
void report_device_identification(char * sysfs_device_dir, int depth) {
   bool debug = false;
   DBGMSF(debug, "sysfs_device_dir: %s", sysfs_device_dir);
   int d1 = depth+1;

   DBGMSF(debug, "Reading device ids from individual attribute files...");
   Device_Ids dev_ids = read_device_ids1(sysfs_device_dir);
#ifdef ALTERNATIVE
   // works, pick one
   DBGMSF(debug, "Reading device ids by parsing modalias attribute...");
   Device_Ids dev_ids2 = read_device_ids2(sysfs_device_dir);
   assert(dev_ids.vendor_id == dev_ids2.vendor_id);
   assert(dev_ids.device_id == dev_ids2.device_id);
   assert(dev_ids.subvendor_id == dev_ids2.subvendor_id);
   assert(dev_ids.subdevice_id == dev_ids2.subdevice_id);
#endif

   bool pci_ids_ok = devid_ensure_initialized();
   if (pci_ids_ok) {
      Pci_Usb_Id_Names names = devid_get_pci_names(
                      dev_ids.vendor_id,
                      dev_ids.device_id,
                      dev_ids.subvendor_id,
                      dev_ids.subdevice_id,
                      4);
      if (!names.vendor_name)
         names.vendor_name = "unknown vendor";
      if (!names.device_name)
         names.device_name = "unknown device";

      rpt_vstring(d1,"Vendor:              x%04x      %s", dev_ids.vendor_id, names.vendor_name);
      rpt_vstring(d1,"Device:              x%04x      %s", dev_ids.device_id, names.device_name);
      if (names.subsys_or_interface_name)
      rpt_vstring(d1,"Subvendor/Subdevice: %04x/%04x  %s", dev_ids.subvendor_id, dev_ids.subdevice_id, names.subsys_or_interface_name);
   }
   else {
      rpt_vstring(d1,"Unable to find pci.ids file for name lookup.");
      rpt_vstring(d1,"Vendor:              %04x       ", dev_ids.vendor_id);
      rpt_vstring(d1,"Device:              %04x       ", dev_ids.device_id);
      rpt_vstring(d1,"Subvendor/Subdevice: %04x/%04x  ", dev_ids.subvendor_id, dev_ids.subdevice_id);
   }
}


/** Returns the name for video class ids.
 *
 *  Hardcoded because device_ids_util.c does not process the class
 *  information that is maintained in file pci.ids.
 *
 *  \param class_id
 *  \return class name, "" if not a display controller class
 */
static char * video_device_class_name(unsigned class_id) {
   char * result = "";
   switch(class_id >> 8) {
   case 0x0300:
      result = "VGA compatible controller";
      break;
   case 0x0301:
      result = "XGA compatible controller";
      break;
   case 0x0302:
      result = "3D controller";
      break;
   case 0x0380:
      result = "Display controller";
      break;
   default:
      if (class_id >> 16 == 0x03)
         result = "Unspecified display controller";
   }
   return result;
}


/** Process attributes of a /sys/bus/pci/devices/nnnn:nn:nn.n directory.\
 *
 *  Ignores non-video devices, i.e. devices whose class does not begin
 *  with x03.
 *
 *  Called by #query_card_and_driver_using_sysfs() via #dir_foreach()
 *
 *  \param  dirname   always /sys/bus/pci/devices
 *  \param  fn        nnnn:nn:nn.n  PCI device path
 *  \param  accum     pointer to accumulator struct, may be NULL
 *  \param  depth     logical indentation depth
 *
 *  \remark
 *  Adds detected driver to list of detected drivers
 */
void each_video_pci_device(
      const char * dirname,
      const char * fn,
      void * accumulator,
      int    depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, fn=%s, accumulator=%p", dirname, fn, accumulator);

   int d1 = depth+1;

   Env_Accumulator * accum = accumulator;
   assert(accum && memcmp(accum->marker, ENV_ACCUMULATOR_MARKER, 4) == 0);

   char cur_dir_name[PATH_MAX];
   sprintf(cur_dir_name, "%s/%s", dirname, fn);
   char * device_class = read_sysfs_attr(cur_dir_name, "class", /*verbose=*/true);
   // DBGMSF(debug, "cur_dir_name=%s, device_class: %s", cur_dir_name, device_class);
   if (!device_class) {
      rpt_vstring(depth, "Unexpected for %s: class not found", cur_dir_name);
      goto bye;
   }
   unsigned class_id = h2uint(device_class);
   // DBGMSF(debug, "class_id: 0x%08x", class_id);
   //   if (str_starts_with(device_class, "0x03")) {
   if (class_id >> 16 == 0x03) {
      bool is_primary_video = false;

      switch(class_id >> 8) {
      case 0x0300:
         is_primary_video=true;
         break;
      case 0x0380:
         break;
      default:
         rpt_vstring(depth, "Unexpected class for video device: %s", device_class);
      }
      char * boot_vga = read_sysfs_attr_w_default(cur_dir_name, "boot_vga", "-1", false);
      // DBGMSG("boot_vga: %s", boot_vga);
      bool boot_vga_flag = (boot_vga && streq(boot_vga, "1")) ;
      rpt_vstring(depth, "%s video controller at PCI address %s (boot_vga flag is %sset)",
                         (is_primary_video) ? "Primary" : "Secondary",
                         fn,
                         (boot_vga_flag) ? "" : "not ");
      rpt_vstring(d1,   "Device class:        x%06x    %s", class_id, video_device_class_name(class_id));
      report_device_identification(cur_dir_name, depth);

      // rpt_nl();
      // rpt_vstring(d1,"Determining driver name and possibly version...");

      char workfn[PATH_MAX];
      g_snprintf(workfn, PATH_MAX, "%s/%s", cur_dir_name, "driver");
      char resolved_path[PATH_MAX];
      // DBGMSF("resulved_path = |%s|", resolved_path);
      char * rpath = realpath(workfn, resolved_path);
      if (!rpath) {
         int errsv = errno;
         if (errsv == ENOENT)
            rpt_vstring(d1, "No driver");
         else {
            rpt_vstring(d1, "realpath(%s) returned NULL, errno=%d (%s)",
                            workfn, errsv, linux_errno_name(errsv));
         }
      }
      else {
         // printf("realpath returned %s\n", rpath);
         // printf("%s --> %s\n",workfn, resolved_path);
         char * rp2 = strdup(rpath);
         // DBGMSF(debug, "Driver name path: rp2 = rpath = %s", rp2);
         char * driver_name = g_path_get_basename(rp2);
         rpt_vstring(d1, "Driver name:         %s", driver_name);
         driver_name_list_add(&accum->driver_list, driver_name);
         free(rp2);
         free(driver_name);

         char driver_module_dir[PATH_MAX];
         g_snprintf(driver_module_dir, PATH_MAX, "%s/driver/module", cur_dir_name);
         // printf("driver_module_dir: %s\n", driver_module_dir);
         char * driver_version = read_sysfs_attr(driver_module_dir, "version", false);
         if (driver_version) {
            rpt_vstring(d1,"Driver version:      %s", driver_version);
            free(driver_version);
         }
         else
            rpt_vstring(d1,"Driver version:      Unable to determine");

         // list associated I2C devices
         dir_foreach(cur_dir_name, NULL, each_video_device_i2c, NULL, d1);
      }
      free(boot_vga);

   }

   else if (str_starts_with(device_class, "0x0a")) {
      rpt_vstring(depth, "Encountered docking station (class 0x0a) device. dir=%s", cur_dir_name);
   }

   free(device_class);

bye:
   return;
}


/** Process attributes of a /sys/bus/platform/drivers directory
 *
 *  Only processes entry for driver vc4_v3d.
 *
 *  Called by #query_card_and_driver_using_sysfs() via #dir_foreach()
 *
 *  \param  dirname   always /sys/bus/platform/drivers
 *  \param  fn        driver name
 *  \param  accum     pointer to accumulator struct, may be NULL
 *  \param  depth     logical indentation depth
 *
 *  \remark
 *  Adds detected driver to list of detected drivers
 */
void each_arm_driver(
      const char *  dirname,
      const char *  fn,
      void *        accumulator,
      int           depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, fn=%s, accumulator=%p", dirname, fn, accumulator);

   Env_Accumulator * accum = accumulator;
   assert(accumulator && memcmp(accum->marker, ENV_ACCUMULATOR_MARKER, 4) == 0);

   if (streq(fn, "vc4_v3d")) {
      const char * driver_name = fn;
      rpt_vstring(depth, "Driver name:    %s", driver_name);
      driver_name_list_add(&accum->driver_list, driver_name);
   }
   DBGMSF(debug, "Done");
}


/** Depending on architecture, examines /sys/bus/pci/devices or
 *  /sub/bus/platform/drivers.
 *
 * \accum   accum
 *
 * \remark
 * Updates list of detected drivers, accum->driver_list
 */
void query_card_and_driver_using_sysfs(Env_Accumulator * accum) {
   bool debug = false;
   DBGMSF(debug, "Starting.  accum=%p", accum);

   rpt_vstring(0,"Obtaining card and driver information from /sys...");
   if (accum->is_arm) {
      DBGMSF(debug, "Machine architecture is %s.  Skipping /sys/bus/pci checks.", accum->architecture);
      char * platform_drivers_dir_name = "/sys/bus/platform/drivers";
      dir_foreach(platform_drivers_dir_name, /*fn_filter*/ NULL, each_arm_driver, accum, 0);
   }
   else {
      char * pci_devices_dir_name = "/sys/bus/pci/devices";
      // each entry in /sys/bus/pci/devices is a symbolic link
      dir_foreach(pci_devices_dir_name, /*fn_filter*/ NULL, each_video_pci_device, accum, 0);
   }
}

// end query_card_and_driver_using_sysfs() section


/** For each driver module name known to be relevant, checks /sys to
 *  see if it is loaded.
 */
void query_loaded_modules_using_sysfs() {
   rpt_vstring(0,"Testing if modules are loaded using /sys...");
   // known_video_driver_modules
   // other_driver_modules

   char ** pmodule_name = get_known_video_driver_module_names();
   char * curmodule;
   int ndx;
   for (ndx=0; (curmodule=pmodule_name[ndx]) != NULL; ndx++) {
      bool is_loaded = is_module_loaded_using_sysfs(curmodule);
      // DBGMSF(debug, "is_loaded=%d", is_loaded);
      rpt_vstring(0,"   Module %-16s is %sloaded", curmodule, (is_loaded) ? "" : "NOT ");
   }
   pmodule_name = get_other_driver_module_names();
   for (ndx=0; (curmodule=pmodule_name[ndx]) != NULL; ndx++) {
      bool is_loaded = is_module_loaded_using_sysfs(curmodule);
      rpt_vstring(0,"   Module %-16s is %sloaded", curmodule, (is_loaded) ? "" : "NOT ");
   }
}


/** Examines a single /sys/bus/i2c/devices/i2c-N directory.
 *
 *  Called by #dir_foreach() from #query_sys_bus_i2c()
 *
 *  \param  dirname     always /sys/bus/i2c/devices
 *  \param  fn          i2c-0, i2c-1, ... (n. these are symbolic links)
 *  \param  accumulator collects environment information
 *  \param  depth       logical indentation depth
 *
 *  \remark
 *  Adds current bus number to **accumulator->sys_bus_i2c_device_numbers
 */
void each_i2c_device(
      const char * dirname,     // always /sys/bus/i2c/devices
      const char * fn,          // i2c-0, i2c-1, ...
      void * accumulator,
      int    depth)
{
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dirname=%s, fn=%s", dirname, fn);
   assert(streq(dirname, "/sys/bus/i2c/devices"));

   Env_Accumulator * accum = accumulator;
   char cur_dir_name[100];
   sprintf(cur_dir_name, "%s/%s", dirname, fn);
   char * dev_name = read_sysfs_attr(cur_dir_name, "name", true);
   char buf[106];
   snprintf(buf, 106, "%s/name:", cur_dir_name);
   rpt_vstring(depth, "%-34s %s", buf, dev_name);
   free(dev_name);

   int busno = i2c_name_to_busno(fn);
   if (busno >= 0) {
      bva_append(accum->sys_bus_i2c_device_numbers, busno);
      accum->sysfs_i2c_devices_exist = true;
   }
   else if (str_ends_with(fn, "-0037")) {
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "ddcci device name: %s", fn);
      accum->sysfs_ddcci_devices_exist = true;
   }
   else {
      // rpt_vstring(depth, "%-34s Unexpected file name: %s", cur_dir_name, fn);
      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "Ignorable /sys/bus/i2c/devices file name: %s", fn);
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


/** Examines /sys/bus/i2c/devices
 *
 *  \param accumulator  collects environment information
 *
 *  \remark
 *  Sets **accumulator->sys_bus_i2c_device_numbers** to sorted
 *  array of detected I2C device numbers.
 */
void query_sys_bus_i2c(Env_Accumulator * accumulator) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   accumulator->sys_bus_i2c_device_numbers = bva_create();
   rpt_vstring(0,"Examining /sys/bus/i2c/devices...");
   char * dname = "/sys/bus/i2c";
   if (!directory_exists(dname)) {
      rpt_vstring(1, "Directory not found: %s", dname);
   }
   else {
      char * dname = "/sys/bus/i2c/devices";
      accumulator->sysfs_i2c_devices_exist = false;
      // each entry in /sys/bus/i2c/devices is a symbolic link
      dir_ordered_foreach(dname, NULL, i2c_compare, each_i2c_device, accumulator, 1);
      if (!accumulator->sysfs_i2c_devices_exist)
         rpt_vstring(1, "No i2c devices found in %s", dname);
      bva_sort(accumulator->sys_bus_i2c_device_numbers);
      if (accumulator->sysfs_ddcci_devices_exist) {
         rpt_vstring(1, "Devices created by driver ddcci found in %s", dname);
         rpt_vstring(1, "Use ddcutil option --force-slave-address to recover from EBUSY errors.");
      }
   }
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


void query_sys_amdgpu_parameters(int depth) {
   const char * parmdir = "/sys/module/drm/holders/amdgpu/parameters";
   int d1 = depth;
   int d2 = depth+1;
   rpt_vstring(d1, "amdgpu parameters (%s)", parmdir);
   if (!directory_exists(parmdir)) {
      rpt_vstring(d2, "Directory not found: %s", parmdir);
   }
   else {
      DIR *dirp;
      struct dirent *dp;

      if ((dirp = opendir(parmdir)) == NULL) {
         int errsv = errno;
         rpt_vstring(d2, "Couldn't open %s. errmp = %d",
                        parmdir, errsv);
      }
      else {
         GPtrArray * sorted_names = g_ptr_array_new();
         while (true){
            dp = readdir(dirp);
            if (!dp)
               break;
            if (dp->d_type & DT_REG) {
               char * fn = dp->d_name;
               g_ptr_array_add(sorted_names, fn);
            }
         }
         g_ptr_array_sort(sorted_names, indirect_strcmp);

         for (int ndx = 0; ndx < sorted_names->len; ndx++) {
            char * fn = g_ptr_array_index(sorted_names, ndx);
            char * value = read_sysfs_attr(parmdir, fn, /*verbose=*/ false);
            char n[100];
            g_snprintf(n, 100, "%s:", fn);
            rpt_vstring(d2, "%-20s  %s", n, value);
         }
         closedir(dirp);
      }
   }
}


/** Examines /sys/class/drm
 */


void insert_drm_xref(int depth, char * cur_dir_name, char * i2c_node_name, Byte * edidbytes) {
   bool debug = false;
   DBGMSF(debug, "depth=%d, cur_dir_name=%s, i2c_node_name = %s, edidbytes=%p",
                 depth, cur_dir_name, i2c_node_name, edidbytes);
   int d2 = depth;
   // rpt_nl();

   int drm_busno =  i2c_path_to_busno(cur_dir_name);
   DBGMSF(debug, "cur_dir_name=%s, drm_busno=%d", cur_dir_name, drm_busno);

   Device_Id_Xref * xref = NULL;
   DBGMSF(debug, "i2c_node_name = %s", i2c_node_name);
   if (i2c_node_name) {
      // DBGMSG("Using i2c_node_name");

      xref = device_xref_find_by_busno(drm_busno);
      if (!xref)
         DBGMSG("Unexpected. Bus %d not in xref table", drm_busno);
   }
   // can we assert that edidbytes != NULL?
   if (!xref && edidbytes) {   // i.e. if !i2c_node_name or lookup by busno failed
      // DBGMSG("searching by EDID");
#ifdef SYSENV_TEST_IDENTICAL_EDIDS
               if (first_edid) {
                  DBGMSG("Forcing duplicate EDID");
                  edidbytes = first_edid;
               }
#endif
      xref = device_xref_find_by_edid(edidbytes);
      if (!xref) {
         char * tag = device_xref_edid_tag(edidbytes);
         DBGMSG("Unexpected. EDID ...%s not in xref table", tag);
         free(tag);
      }
   }
   if (xref) {
      if (xref->ambiguous_edid) {
         rpt_vstring(d2, "Multiple displays have same EDID ...%s", xref->edid_tag);
         rpt_vstring(d2, "drm name, and bus number in device cross reference table may be incorrect");
      }
      // xref->sysfs_drm_name = strdup(dent->d_name);
      xref->sysfs_drm_name = strdup(cur_dir_name);
      xref->sysfs_drm_i2c  = i2c_node_name;
      xref->sysfs_drm_busno = drm_busno;
      DBGMSF(debug, "sysfs_drm_busno = %d", xref->sysfs_drm_busno);
   }
}


void report_one_connector(
      const char * dirname,     // <device>/drm/cardN
      const char * simple_fn,   // card0-HDMI-1 etc
      void *       data,
      int          depth)
{
   bool debug = false;
   int d1 = depth+1;
   int d2 = depth+2;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);

   rpt_nl();
   rpt_vstring(depth, "Connector: %s", simple_fn);

   // rpt_nl();
   GByteArray * edid_byte_array;
   RPT_ATTR_TEXT(d1, NULL, dirname, simple_fn, "enabled");
   RPT_ATTR_TEXT(d1, NULL, dirname, simple_fn, "status");
   RPT_ATTR_EDID(d1, &edid_byte_array, dirname, simple_fn, "edid");

   char * i2c_subdir_name = NULL;
   GET_ATTR_SINGLE_SUBDIR(&i2c_subdir_name, is_i2cN, NULL, dirname, simple_fn);
   // rpt_vstring(d1, "i2c_device: %s", i2c_subdir_name);

   if (i2c_subdir_name) {
      char * node_name = NULL;
      rpt_vstring(d1, "i2c_device: %s", i2c_subdir_name);
      RPT_ATTR_TEXT(d2, &node_name, dirname, simple_fn, i2c_subdir_name, "name");

      i2c_path_to_busno(i2c_subdir_name);

      Byte * edid_bytes = NULL;
      if (edid_byte_array) {
         edid_bytes = g_byte_array_free(edid_byte_array, false);
      }
      insert_drm_xref(d2, i2c_subdir_name, node_name, edid_bytes);
      free(edid_bytes);
   }
   else
      rpt_vstring(d2, "No i2c-N subdirectory");
   DBGMSF(debug, "Done");
}


void query_drm_using_sysfs()
{
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   int depth = 1;
   int d0 = depth;
   rpt_nl();
   char * dname =
#ifdef TARGET_BSD
             "/compat/linux/sys/class/drm";
#else
             "/sys/class/drm";
#endif

   rpt_vstring(d0, "*** Examining %s ***", dname);
   dir_filtered_ordered_foreach(
                dname,
                is_card_connector_dir,   // filter function
                NULL,                    // ordering function
                report_one_connector,
                NULL,                    // accumulator
                depth);
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}

//
// Functions for probing /sys
//


void show_top_level_sys_entries(int depth) {
   rpt_label(depth, "*** Character device major numbers of interest: ***");
   char * names[] = {"i2c", "drm", "ddcci", "aux", "vfio", NULL};
   GPtrArray * filtered_proc_devices = g_ptr_array_new();
   read_file_with_filter(
             filtered_proc_devices,
             "/proc/devices",
             names,
             false,     // ignore_case,
             0);        // no limit

   for (int ndx = 0; ndx < filtered_proc_devices->len; ndx++) {
     rpt_label(depth+1, g_ptr_array_index(filtered_proc_devices, ndx));
   }
   rpt_nl();

   rpt_label(depth, "*** Top Level I2C Related Nodes ***");
   rpt_nl();
   char * cmds[] = {
#ifdef NOT_USEFUL
     "ls -l /sys/bus/pci_express/devices",
     "ls -l /sys/devices/pci*",
     "ls -l /sys/class/pci*",             // pci_bus, pci_epc
#endif
     "ls -l /sys/class/backlight:*",
     "ls -l /sys/bus/pci/devices",
     "ls -l /sys/bus/i2c/devices",
     "ls -l /sys/bus/platform/devices",   // not symbolic links
     "ls -l /sys/class/drm*",             // drm, drm_dp_aux_dev
     "ls -l /sys/class/i2c*",             // i2c, i2c-adapter, i2c-dev
     "ls -l /sys/class/vfio*",
     NULL
   };

   int ndx = 0;
   while ( cmds[ndx] ) {
      rpt_vstring(1, "%s ...", cmds[ndx]);
      execute_shell_cmd_rpt(cmds[ndx],depth + 2);
      ndx++;
      rpt_nl();
   }

   for (int ndx = 0; ndx < filtered_proc_devices->len; ndx++) {
      char * name = NULL;
      int    number = 0;
      if (sscanf(g_ptr_array_index(filtered_proc_devices, ndx),
                 "%d %ms", &number, &name)  == 2) { // allow for pathological case of invalid /proc data
         // DBGMSG("number = %d, name = %s"show_top_level_sys_entries, number, name);
         char cmd[100];
         snprintf(cmd, 100,       "ls -l /sys/dev/char/%d:*", number);
         // DBGMSG("cmd: %s", cmd);
         rpt_vstring(depth+1, "Char major: %d %s", number, name);
         execute_shell_cmd_rpt(cmd,depth + 2);
         free(name);
         rpt_nl();
      }
   }

   g_ptr_array_free(filtered_proc_devices, true);
}


#ifdef FUTURE
// 9/28/2021 Requires hardening, testing on other than amdgpu, MST etc

typedef struct {
   char * connector_name;
   int    i2c_busno;
   bool   is_aux_channel;
   int    base_busno;
   char * name;
   char * base_name;
   Byte * edid_bytes;
   gsize  edid_size;
   bool   enabled;
   char * status;
} Sys_Drm_Display;


void free_sys_drm_display(void * display) {
   if (display) {
      Sys_Drm_Display * disp = display;
      free(disp->connector_name);
      free(disp->edid_bytes);
      free(disp->name);
      free(disp->status);
      free(disp);
   }
}


static GPtrArray * sys_drm_displays = NULL;  // Sys_Drm_Display;

// typedef Dir_Filter_Func
bool is_drm_connector(const char * dirname, const char * simple_fn) {
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, simple_fn=%s", dirname, simple_fn);
   bool result = false;
   if (str_starts_with(simple_fn, "card")) {
      char * s0 = strdup( simple_fn + 4);   // work around const char *
      char * s = s0;
      while (isdigit(*s)) s++;
      if (*s == '-')
         result = true;
      free(s0);
   }
   DBGMSF(debug, "Done.     Returning %s", SBOOL(result));
   return result;
}


bool fn_starts_with(const char * filename, const char * val) {
   return str_starts_with(filename, val);
}


// typedef Dir_Foreach_Func
void one_drm_connector(
      const char *  dirname,
      const char *  fn,
      void *        accumulator,
      int           depth)
{
   bool debug = false;
   DBGMSF(debug, "dirname=%s, fn=%s, depth=%d", dirname, fn, depth);
   assert( accumulator == sys_drm_displays);   // temp
   Sys_Drm_Display * cur = calloc(1, sizeof(Sys_Drm_Display));
   g_ptr_array_add(sys_drm_displays, cur);
   cur->connector_name = strdup(fn);
   char * b1 = NULL;
   RPT_ATTR_TEXT(depth,&b1, dirname, fn, "enabled");
   cur->enabled = streq(b1, "enabled");
   RPT_ATTR_TEXT(depth, &cur->status,  dirname, fn, "status");

   GByteArray * edid_byte_array = NULL;
   rpt_attr_edid(depth, &edid_byte_array, dirname, fn, "edid");
   if (edid_byte_array) {
      cur->edid_bytes = g_byte_array_steal(edid_byte_array, &cur->edid_size);
   }

   bool has_drm_dp_aux_subdir =
         RPT_ATTR_SINGLE_SUBDIR(
               depth,
               NULL,     // char **      value_loc,
               fn_starts_with,
               "drm_dp_aux",
               dirname,
               fn);

   char * buf = NULL;
   bool has_i2c_subdir =
         RPT_ATTR_SINGLE_SUBDIR(depth, &buf, fn_starts_with,"i2c-",
                                dirname, fn);

   ASSERT_IFF(has_drm_dp_aux_subdir, has_i2c_subdir);
   cur->is_aux_channel = has_drm_dp_aux_subdir;
   if (has_i2c_subdir) {  // DP
      cur->base_busno = i2c_name_to_busno(buf);
      RPT_ATTR_TEXT(depth, &cur->name, dirname, fn, buf, "name");

      RPT_ATTR_TEXT(depth, &cur->base_name, dirname, fn, "ddc", "name");

      has_i2c_subdir =
            RPT_ATTR_SINGLE_SUBDIR(depth, &buf, fn_starts_with, "i2c-",
                                   dirname, fn, "ddc", "i2c-dev");
      if (has_i2c_subdir) {
         cur->base_busno = i2c_name_to_busno(buf);
      }
   }
   else {   // not DP
      RPT_ATTR_TEXT(depth, &cur->name, dirname, fn, "ddc", "name");

      has_i2c_subdir = RPT_ATTR_SINGLE_SUBDIR(
            depth,
            &buf,
            fn_starts_with,
            "i2c-",
            dirname,
            fn,
            "ddc",
            "i2c-dev");
      if (has_i2c_subdir) {
         cur->i2c_busno = i2c_name_to_busno(buf);
      }
   }
}


void scan_sys_drm_displays(int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting");
   int d0 = depth;
   int d1 = depth+1;
   rpt_nl();
   rpt_label(d0, "Scanning /sys/class/drm for /dev/i2c device numbers and EDIDs");
   if (sys_drm_displays) {
      g_ptr_array_free(sys_drm_displays, true);
      sys_drm_displays = NULL;
   }
   sys_drm_displays = g_ptr_array_new_with_free_func(free_sys_drm_display);

   dir_filtered_ordered_foreach("/sys/class/drm",
                       is_drm_connector,      // filter function
                       NULL,                    // ordering function
                       one_drm_connector,
                       sys_drm_displays,                    // accumulator
                       d1);

   DBGMSF(debug, "Done");
}


void report_sys_drm_displays(int depth) {
   int d0 = depth;
   int d1 = depth+1;
   rpt_nl();
   rpt_label(d0, "Displays via DRM connector:");
   if (!sys_drm_displays)
     scan_sys_drm_displays(depth);
   GPtrArray * displays = sys_drm_displays;
   if (!displays || displays->len == 0) {
      rpt_label(d1, "None");
   }
   else {
      for (int ndx = 0; ndx < displays->len; ndx++) {
         Sys_Drm_Display * cur = g_ptr_array_index(displays, ndx);
         rpt_nl();
         rpt_vstring(d0, "Connector: %s", cur->connector_name);
         rpt_vstring(d1, "i2c_busno: %d", cur->i2c_busno);
         rpt_vstring(d1, "enabled:   %s", sbool(cur->enabled));
         rpt_vstring(d1, "status:    %s", cur->status);
         rpt_vstring(d1, "name:      %s", cur->name);
         rpt_label(d1,   "edid:");
         rpt_hex_dump(cur->edid_bytes, cur->edid_size, d1);
         if (cur->is_aux_channel) {
            rpt_vstring(d1, "base_busno:    %d", cur->base_busno);
            rpt_vstring(d1, "base_name:     %s", cur->base_name);
         }
      }
   }
}
#endif


/** Master function for dumping /sys directory
 */
void dump_sysfs_i2c() {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "");
   rpt_nl();

   rpt_label(0, "Dumping sysfs i2c entries");

   rpt_nl();
   show_top_level_sys_entries(0);
   // dump_original_sys_scans();
   // dump_simplified_sys_bus_pci(0);
   dump_detailed_sys_bus_pci(0);
#ifdef FUTURE
   report_sys_drm_displays(0);
#endif
   DBGTRC_DONE(debug, TRACE_GROUP, "");
}


void init_query_sysfs() {
   RTTI_ADD_FUNC(query_drm_using_sysfs);
   RTTI_ADD_FUNC(query_sys_bus_i2c);
   RTTI_ADD_FUNC(each_i2c_device);
   RTTI_ADD_FUNC(dump_sysfs_i2c);

   init_query_detailed_bus_pci_devices();
}

