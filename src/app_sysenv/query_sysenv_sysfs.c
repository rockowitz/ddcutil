/** @file query_sysenv_sysfs.c
 *
 *  Query environment using /sys file system
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


/** \cond */
// #define _GNU_SOURCE   // for asprintf

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <libgen.h>
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
#include "util/sysfs_util.h"

#include "base/core.h"
#include "base/linux_errno.h"
/** \endcond */

#include "query_sysenv_base.h"
#include "query_sysenv_xref.h"

#include "query_sysenv_sysfs.h"

// Notes on directory structure
//
//  /sys/bus/pci/devices/0000:nn:nn.n/
//        boot_vga   1  if the boot device, appears not exist ow
//        class      0x030000 for video
//        device     hex PID
//        driver    -> /sys/bus/pci/drivers/radeon
//        drm
//           card0 (dir)
//           controlD64 (dir)
//           controlD128 (dir)
//        enable
//        graphics (dir)
//            fb0 (dir)
//        i2c-n (dir)
//            device -> /sys/bus/pci/devices/0000:nn:nn.n
//            name
//        modalias
//        subsystem (dir)  -> /sys/bus/pci
//             devices (dir)
//             drivers (dir)
//        subsystem_device
//        subsystem_vendor
//        vendor           hex VID
//
// also of possible interest:
// /sys/class/i2c-dev/i2c-*/name
//    refers to video driver or piix4_smbus
// also accessed at:
// /sys/bus/i2c/devices/i2c-*/name
// /sys/bus/pci/drivers/nouveau
// /sys/bus/pci/drivers/piix4_smbus
// /sys/bus/pci/drivers/nouveau/0000:01:00.0
//                                           /name
//                                           i2c-dev
// /sys/module/nvidia
// /sys/module/i2c_dev ?
// /sys/module/... etc

// Raspbian:
// /sys/bus/platform/drivers/vc4_v3d
// /sys/module/vc4


// Local conversion functions for data coming from sysfs,
// which should always be valid.

static ushort h2ushort(char * hval) {
   bool debug = false;
   int ct;
   ushort ival;
   ct = sscanf(hval, "%hx", &ival);
   assert(ct == 1);
   if (debug)
      DBGMSG("hhhh = |%s|, returning 0x%04x", hval, ival);
   return ival;
}

static unsigned h2uint(char * hval) {
   bool debug = false;
   int ct;
   unsigned ival;
   ct = sscanf(hval, "%x", &ival);
   assert(ct == 1);
   if (debug)
      DBGMSG("hhhh = |%s|, returning 0x%08x", hval, ival);
   return ival;
}



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
void each_video_device_i2c(char * dirname, char * fn, void * accumulator, int depth) {
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
 *  \param sysfs_device_dir   always /sys/bus/pck/devices/nnnn:nn:nn.n
 *  \param depth              logical indentation depth
 */
void report_device_identification(char * sysfs_device_dir, int depth) {
   bool debug = false;
   DBGMSF(debug, "sysfs_device_dir: %s", sysfs_device_dir);
   int d1 = depth+1;

   DBGMSF0(debug, "Reading device ids from individual attribute files...");
   Device_Ids dev_ids = read_device_ids1(sysfs_device_dir);
   DBGMSF0(debug, "Reading device ids by parsing modalias attribute...");
   Device_Ids dev_ids2 = read_device_ids2(sysfs_device_dir);
   assert(dev_ids.vendor_id == dev_ids2.vendor_id);
   assert(dev_ids.device_id == dev_ids2.device_id);
   assert(dev_ids.subvendor_id == dev_ids2.subvendor_id);
   assert(dev_ids.subdevice_id == dev_ids2.subdevice_id);

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
 *  Hardcoded because device_ids_util.c does not maintain the class
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
      char * dirname,
      char * fn,
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
   // DBGMSF(debug, "cur_dir_name: %s", cur_dir_name);
   char * device_class = read_sysfs_attr(cur_dir_name, "class", /*verbose=*/true);
   // DBGMSF(debug, "device_class: %s", device_class);
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
void each_arm_driver(char * dirname, char * fn, void * accumulator, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, fn=%s, accumulator=%p", dirname, fn, accumulator);

   Env_Accumulator * accum = accumulator;
   assert(accumulator && memcmp(accum->marker, ENV_ACCUMULATOR_MARKER, 4) == 0);

   if (streq(fn, "vc4_v3d")) {
      char * driver_name = fn;
      rpt_vstring(depth, "Driver name:    %s", driver_name);
      driver_name_list_add(&accum->driver_list, driver_name);
   }
   DBGMSF0(debug, "Done");
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
 *  \param  dirname     always /sys/bus/i2c/devicesS
 *  \param  fn          i2c-0, i2c-1, ... (n. these are symbolic links)
 *  \param  accumulator collects environment information
 *  \param  depth       logical indentation depth
 *
 *  \remark
 *  Adds current bus number to **accumulator->sys_bus_i2c_device_numbers
 */
void each_i2c_device(
      char * dirname,     // always /sys/bus/i2c/devices
      char * fn,          // i2c-0, i2c-1, ...
      void * accumulator,
      int    depth)
{
   bool debug = false;
   assert(streq(dirname, "/sys/bus/i2c/devices"));
   DBGMSF(debug, "dirname=|%s|, fn=|%s|", dirname, fn);
   Env_Accumulator * accum = accumulator;
   char cur_dir_name[100];
   sprintf(cur_dir_name, "%s/%s", dirname, fn);
   char * dev_name = read_sysfs_attr(cur_dir_name, "name", true);
   char buf[106];
   snprintf(buf, 106, "%s/name:", cur_dir_name);
   rpt_vstring(depth, "%-34s %s", buf, dev_name);
   free(dev_name);

   int busno = i2c_name_to_busno(fn);
   if (busno >= 0)
      bva_append(accum->sys_bus_i2c_device_numbers, busno);
   else
      DBGMSG("Unexpected /sys/bus/i2c/devices file name: %s", fn);

   accum->sysfs_i2c_devices_exist = true;
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
      dir_foreach(dname, NULL, each_i2c_device, accumulator, 1);
      if (!accumulator->sysfs_i2c_devices_exist)
         rpt_vstring(1, "No i2c devices found in %s", dname);
      bva_sort(accumulator->sys_bus_i2c_device_numbers);
   }
}


int indirect_strcmp(const void * a, const void * b) {
   char * alpha = *(char **) a;
   char * beta  = *(char **) b;
   return strcmp(alpha, beta);
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
void query_drm_using_sysfs()
{
   struct dirent *dent;
   struct dirent *dent2;
   DIR           *dir1;
   char          *dname;
   char          dnbuf[90];
   const int     cardname_sz = 20;
   char          cardname[cardname_sz];

   int depth = 0;
   int d1    = depth+1;
   int d2    = depth+2;

   bool debug = false;

   rpt_vstring(depth,"Examining /sys/class/drm...");
   dname = "/sys/class/drm";
   dir1 = opendir(dname);
   if (!dir1) {
      rpt_vstring(d1, "drm not defined in sysfs. Unable to open directory %s: %s\n",
                     dname, strerror(errno));
   }
   else {
      closedir(dir1);
      int cardno = 0;
      for (;;cardno++) {
         snprintf(cardname, cardname_sz, "card%d", cardno);
         snprintf(dnbuf, 80, "/sys/class/drm/%s", cardname);
         dir1 = opendir(dnbuf);
         if (!dir1) {
            // rpt_vstring(d1, "Unable to open sysfs directory %s: %s\n", dnbuf, strerror(errno));
            break;
         }
         else {
            while ((dent = readdir(dir1)) != NULL) {
               // DBGMSG("%s", dent->d_name);
               // char cur_fn[100];
               if (str_starts_with(dent->d_name, cardname)) {
                  rpt_vstring(d1, "Found connector: %s", dent->d_name);
                  char cur_dir_name[PATH_MAX];
                  g_snprintf(cur_dir_name, PATH_MAX, "%s/%s", dnbuf, dent->d_name);

                  // attribute dpms always "On"
                  // char * s_dpms = read_sysfs_attr(cur_dir_name, "dpms", false);
                  // rpt_vstring(d1, "%s/dpms: %s", cur_dir_name, s_dpms);

                  // attribute enabled always "disabled"
                  // char * s_enabled = read_sysfs_attr(cur_dir_name, "enabled", false);
                  // rpt_vstring(d1, "%s/enabled: %s", cur_dir_name, s_enabled);

                  char * s_status = read_sysfs_attr(cur_dir_name, "status", false);
                  rpt_vstring(d2, "%s/status: %s", cur_dir_name, s_status);
                  // edid present iff status == "connected"
                  if (streq(s_status, "connected")) {
                     GByteArray * gba_edid = read_binary_sysfs_attr(
                           cur_dir_name, "edid", 128, /*verbose=*/ false);

                     // hex_dump(gba_edid->data, gba_edid->len);

#ifdef UNNEEDED
                    rpt_vstring(d2, "Raw EDID:");
                    rpt_hex_dump(gba_edid->data, gba_edid->len, 2);
                    if (gba_edid->len >= 128) {
                       Parsed_Edid * parsed_edid = create_parsed_edid(gba_edid->data);
                       if (parsed_edid) {
                          report_parsed_edid_base(
                             parsed_edid,
                             true,   // verbose
                             false,  // show_hex
                             2);     // depth
                          free_parsed_edid(parsed_edid);

                       }
                       else {
                           rpt_vstring(d2, "Unable to parse EDID");
                           // printf(" Unparsable EDID for output name: %s -> %p\n", prec->output_name, prec->edidbytes);
                           // hex_dump(prec->edidbytes, 128);
                       }
                    }
#endif

                    // look for i2c-n subdirectory, may or may not be present depending on driver
                    // DBGMSG("cur_dir_name: %s", cur_dir_name);
                    DIR* dir2 = opendir(cur_dir_name);
                    char * i2c_node_name = NULL;

                    if (!dir2) {
                       rpt_vstring(d1, "Unexpected error. Unable to open sysfs directory %s: %s\n",
                                      cur_dir_name, strerror(errno));
                       break;
                    }
                    else {
                       while ((dent2 = readdir(dir2)) != NULL) {
                          // DBGMSG("%s", dent2->d_name);
                          if (str_starts_with(dent2->d_name, "i2c")) {
                             rpt_vstring(d2, "I2C device: %s", dent2->d_name);
                             i2c_node_name = strdup(dent2->d_name);
                             break;
                          }
                       }
                       closedir(dir2);
                    }

                    // rpt_nl();

                    Device_Id_Xref * xref = NULL;
                    DBGMSF(debug, "i2c_node_name = %s", i2c_node_name);
                    if (i2c_node_name) {
                       // DBGMSG("Using i2c_node_name");
                       int drm_busno =  i2c_path_to_busno(i2c_node_name);
                       xref = device_xref_find_by_busno(drm_busno);
                       if (!xref)
                          DBGMSG("Unexpected. Bus %d not in xref table", drm_busno);
                    }
                    if (!xref) {   // i.e. if !i2c_node_name or lookup by busno failed
                       // DBGMSG("searching by EDID");
                       Byte * edidbytes = gba_edid->data;
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
                       xref->sysfs_drm_busno = i2c_path_to_busno(i2c_node_name);
                       DBGMSF(debug, "sysfs_drm_busno = %d", xref->sysfs_drm_busno);
                    }

                    g_byte_array_free(gba_edid, true);
                 }
                 rpt_nl();
               }
            }
            closedir(dir1);
         }
      }
      if (cardno==0)
         rpt_vstring(d1, "No drm class cards found in %s", dname);
   }

   rpt_title("Query file system for i2c nodes under /sys/class/drm/card*...", 1);
   execute_shell_cmd_rpt("ls -ld /sys/class/drm/card*/card*/i2c*", 1);
}


static inline char *
read_sysfs_attr0(
      const char * fq_attrname,
      bool         verbose)
{
   return file_get_first_line(fq_attrname, verbose);
}


typedef bool (*Fn_Filter)(const char * fn, const char * val);

char * get_single_subdir_name(char * dirname, Fn_Filter filter, char * val) {
   bool debug = false;
   int d1 = 1;
   DIR* dir2 = opendir(dirname);
   char * result = NULL;
   if (!dir2) {
      rpt_vstring(d1, "Unexpected error. Unable to open sysfs directory %s: %s\n",
                      dirname, strerror(errno));
     }
     else {
        struct dirent *dent2;
        while ((dent2 = readdir(dir2)) != NULL) {
           DBGMSF(debug, "%s", dent2->d_name);
           if (!str_starts_with(dent2->d_name, ".")) {
              if (!filter || filter(dent2->d_name, val)) {
                 result = strdup(dent2->d_name);
                 break;
              }
           }
        }
        closedir(dir2);
     }
   DBGMSF(debug, "directory: %s, first subdir: %s", dirname, result);
   return result;
}




// Expecting:

//      device/ddc/i2c-dev/i2c-n   name of directory, associated i2c
//      device/ddc/i2c-dev/i2c-n/name  e.g. AMDGPU i2c bit bus x90
//      device/drm_dp_aux0
//      device/drm_dp_auxN/dev  = 237:M
//      device/name (null)
//   /sys/bus/i2c/devices/i2c-N/device   basename startswith card
//   /sys/bus/i2c/devices/i2c-N/i2c-dev/i2c-N   directory name
//   /sys/bus/i2c/devices/i2c-N/name  = cardN-connector-name
//   /sys/bus/i2c/devices/i2c-N/device
//   /sys/bus/i2c/devices/i2c-N/device/ddc/i2c-dev/i2c-M  name of associated i2c
//   /sys/bus/i2c/devices/i2c-N/device/drm_dp_auxN/

//   /sys/bus/i2c/devices/i2c-N/device/ddc



void rpt_attr_output(
      int depth,
      const char * node,
      const char * op,
      const char * value)
{
   int offset = 60;
   rpt_vstring(depth, "%-*s%-2s %s", offset, node, op, value);
}


void assemble_sysfs_path(
      char * buffer,
      int    bufsz,
      const char * part1,
      const char * part2,
      const char * part3,
      const char * part4)
{
   if (part1 && part2 && part3 && part4)
      g_snprintf(buffer, bufsz, "%s/%s/%s/%s", part1, part2, part3, part4);
   else if (part1 && part2 && part3)
         g_snprintf(buffer, bufsz, "%s/%s/%s", part1, part2, part3);
   else if (part1 && part2)
         g_snprintf(buffer, bufsz, "%s/%s", part1, part2);
   else {
      assert(part1);
      g_snprintf(buffer, bufsz, "%s", part1);
   }
}


bool rpt_attr_text(
      int          depth,
      const char * dir_devices_i2cN,
      const char * attribute,
      char **      value_loc)
{
   bool found = false;
   if (value_loc)
      *value_loc = NULL;
   char pb1[PATH_MAX];
   g_snprintf(pb1, PATH_MAX, "%s/%s", dir_devices_i2cN, attribute);
   char * val = read_sysfs_attr0(pb1, false);
   if (val) {
      found = true;
      rpt_attr_output(depth, pb1, "=", val);
      // rpt_vstring(depth, "%-*s =  %s", offset, pb1, val);
      if (value_loc)
         *value_loc = val;
      else
         free(val);
   }
   else
      rpt_attr_output(depth, pb1, ": ", "Not Found");
   return found;
}


bool
rpt_subdir_attr_text(
      int          depth,
      const char * dir_devices_i2cN,
      const char * subdir_prefix,
      const char * subdir,
      const char * subdir_suffix,
      char **      value_loc)
{
   bool found = false;
   if (value_loc)
      *value_loc = NULL;
   char pb2[PATH_MAX];
   assemble_sysfs_path(pb2, PATH_MAX, dir_devices_i2cN, subdir_prefix, subdir, subdir_suffix);

   char * val = read_sysfs_attr0(pb2, true);
   if (val) {
      rpt_attr_output(depth, pb2, "=", val);
      // rpt_vstring(depth, "%-*s =  %s", offset, pb2, val);
      if (value_loc)
         *value_loc = val;
      else
         free(val);
   }
   else
      rpt_attr_output(depth, pb2, ": ", "Not Found");
   return found;
}


bool
rpt_attr_realpath(
      int          depth,
      const char * part1,
      const char * part2,
      const char * part3,
      const char * part4,
      char **      value_loc)
{
   if (value_loc)
      *value_loc = NULL;
   char pb1[PATH_MAX];
   assemble_sysfs_path(pb1, PATH_MAX, part1, part2, part3, part4);
   char * result = realpath(pb1, NULL);
   bool found = (result);
   if (result) {
      rpt_attr_output(depth, pb1, "->", result);
      // rpt_vstring(depth, "%-*s -> %s", offset, pb1, result);
      if (value_loc)
         *value_loc = result;
      else
         free(result);
   }
   else {
      rpt_attr_output(depth, pb1, "->", "Invalid path");
      // rpt_vstring(depth, "%-*s -> %s", offset, pb1, "Invalid path");
   }
   return found;
}


bool
rpt_attr_realpath_basename(
      int          depth,
      const char * part1,
      const char * part2,
      const char * part3,
      const char * part4,
      char **      value_loc)
{
   bool found = false;
   if (value_loc)
      *value_loc = NULL;
   char pb1[PATH_MAX];
   char pb2[PATH_MAX];
   assemble_sysfs_path(pb1, PATH_MAX, part1, part2, part3, part4);
   char * rpath = realpath(pb1, pb2);  // without assignment, get warning that return value unused
   if (rpath) {
      char * bpath = basename(rpath);
      if (bpath) {
         found = true;
         rpt_attr_output(depth, pb1, "->", bpath);
         // rpt_vstring(depth, "%-*s -> %s", offset, pb1, bpath);
         if (value_loc)
            *value_loc = strdup(bpath);
      }
   }
   if (!found) {
      rpt_attr_output(depth, pb1, "->", "Invalid path");
      // rpt_vstring(depth, "%-*s -> %s", offset, pb1, "Invalid path");
   }
   return found;
}


bool rpt_attr_single_subdir(
      int          depth,
      const char * dir_devices_i2cN,
      const char * filename,
      char **      value_loc)
{
   char pb1[PATH_MAX];
   g_snprintf(pb1, PATH_MAX, "%s/%s", dir_devices_i2cN, filename);
   char * subdir_name = get_single_subdir_name(pb1, NULL, NULL);
   bool found = (subdir_name);
   if (subdir_name) {
      char buf[PATH_MAX+100];
      g_snprintf(buf, PATH_MAX+100, "Found subdirectory = %s", subdir_name);
      rpt_attr_output(depth, pb1, ":", buf);
      // rpt_vstring(depth, "%-*s :  Found subdirectory = %s", offset, pb1, subdir_name);
      if (value_loc)
         *value_loc = subdir_name;
      else
         free(subdir_name);
   }
   else {
      rpt_attr_output(depth, pb1, ":", "No subdirectory found");
      // rpt_vstring(depth, "%-*s :  No subdirectory found", offset, pb1);
   }
   return found;
}


void one_bus_i2c_device(int busno, void * accumulator, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d", busno);

   int d1 = depth+1;
   // int d2 = depth+2;
   // int d3 = depth+3;
   char pb1[PATH_MAX];
   char pb2[PATH_MAX];

   int offset = 60;

   char * dir_devices_i2cN = g_strdup_printf("/sys/bus/i2c/devices/i2c-%d", busno);
   char * real_device_dir = realpath(dir_devices_i2cN, pb2);
   rpt_vstring(depth, "Examining %s -> %s", dir_devices_i2cN, real_device_dir);

#ifdef OLD
   g_snprintf(pb1, PATH_MAX, "%s/device", dir_devices_i2cN);
   rpt_vstring(d1, "%-*s -> %s", offset, pb1, realpath(pb1, pb2));
#endif

   rpt_attr_realpath(d1, dir_devices_i2cN, "device", NULL, NULL, NULL);
   rpt_attr_text(    d1, dir_devices_i2cN, "name",   NULL);

#ifdef OLD
   g_snprintf(pb1, PATH_MAX, "%s/name", dir_devices_i2cN);
   char * val = read_sysfs_attr0(pb1, true);
   rpt_vstring(d1, "%-*s =  %s", offset, pb1, val);
   free(val);
#endif

   char * device_class = NULL;
   // bool found = false;
   if ( rpt_attr_text(d1, dir_devices_i2cN, "device/class", &device_class) ) {

#ifdef OLD
   g_snprintf(pb1, PATH_MAX, "%s/device/class", dir_devices_i2cN);
   char * device_class = read_sysfs_attr0(pb1,  /*verbose=*/false);
   if (device_class) {
       // DBGMSG("class = %s", device_class);
      rpt_vstring(d1, "%-*s =  %s", offset,  pb1, device_class);
#endif

      if ( str_starts_with(device_class, "0x03") ){   // TODO: replace test

         rpt_attr_text(d1, dir_devices_i2cN, "device/boot_vga", NULL);
#ifdef OLD
         g_snprintf(pb1, PATH_MAX, "%s/device/boot_vga", dir_devices_i2cN);
         char * value = read_sysfs_attr0(pb1, /*verbose=*/false);
         rpt_vstring(d1, "%-*s =  %s", offset, pb1, value);
         free(value);
#endif

         rpt_attr_realpath_basename( d1, dir_devices_i2cN, "device/driver", NULL, NULL, NULL);
         rpt_attr_realpath_basename( d1, dir_devices_i2cN, "device/driver/module", NULL, NULL, NULL);
#ifdef OLD
         g_snprintf(pb1, PATH_MAX, "%s/device/driver", dir_devices_i2cN);
         char * fq_driver_path =  realpath(pb1, NULL);
         rpt_vstring(d1, "%-*s -> %s",  offset, pb1, basename(fq_driver_path) );
         free(fq_driver_path);


         g_snprintf(pb1, PATH_MAX, "%s/device/driver/module", dir_devices_i2cN);
         fq_driver_path =  realpath(pb1, NULL);
         rpt_vstring(d1, "%-*s -> %s",  offset, pb1, basename(fq_driver_path) );
         free(fq_driver_path);
#endif
         rpt_attr_text(d1, dir_devices_i2cN, "device/enable",           NULL);
         rpt_attr_text(d1, dir_devices_i2cN, "device/modalias",         NULL);
         rpt_attr_text(d1, dir_devices_i2cN, "device/vendor",           NULL);
         rpt_attr_text(d1, dir_devices_i2cN, "device/device",           NULL);
         rpt_attr_text(d1, dir_devices_i2cN, "device/subsystem_vendor", NULL);
         rpt_attr_text(d1, dir_devices_i2cN, "device/subsystem_device", NULL);

#ifdef OLD
         g_snprintf(pb1, PATH_MAX, "%s/device/enable", dir_devices_i2cN);
         value = read_sysfs_attr0(pb1, /*verbose=*/false);
         rpt_vstring(d1, "%-*s :  %s", offset, pb1, value);
         free(value);

         g_snprintf(pb1, PATH_MAX, "%s/device/modalias", dir_devices_i2cN);
         value = read_sysfs_attr0(pb1, /*verbose=*/false);
         rpt_vstring(d1, "%-*s :  %s", offset, pb1, value);
         free(value);

         g_snprintf(pb1, PATH_MAX, "%s/device/vendor", dir_devices_i2cN);
         value = read_sysfs_attr0(pb1, /*verbose=*/false);
         rpt_vstring(d1, "%-*s :  %s", offset, pb1, value);
         free(value);

         g_snprintf(pb1, PATH_MAX, "%s/device/device", dir_devices_i2cN);
         value = read_sysfs_attr0(pb1, /*verbose=*/false);
         rpt_vstring(d1, "%-*s :  %s", offset, pb1, value);
         free(value);

         g_snprintf(pb1, PATH_MAX, "%s/device/subsystem_vendor", dir_devices_i2cN);
         value = read_sysfs_attr0(pb1, /*verbose=*/false);
         rpt_vstring(d1, "%-*s :  %s", offset, pb1, value);
         free(value);

         g_snprintf(pb1, PATH_MAX, "%s/device/subsystem_device", dir_devices_i2cN);
         value = read_sysfs_attr0(pb1, /*verbose=*/false);
         rpt_vstring(d1, "%-*s :  %s", offset, pb1, value);
         free(value);
#endif

         char * i2c_dev_subdir = NULL;
         rpt_attr_single_subdir(d1, dir_devices_i2cN, "i2c-dev", &i2c_dev_subdir);
#ifdef OLD
         g_snprintf(pb1, PATH_MAX, "%s/i2c-dev", dir_devices_i2cN);
         char * i2c_dev_subdir = get_single_subdir_name(pb1, NULL, NULL);
         if (i2c_dev_subdir)
            rpt_vstring(d1, "%-*s :  Found subdirectory = %s", offset, pb1, i2c_dev_subdir);
         else
            rpt_vstring(d1, "%-*s :  No subdirectory found", offset, pb1);
#endif
         if (i2c_dev_subdir) {
            rpt_subdir_attr_text(d1, dir_devices_i2cN, "i2c-dev", i2c_dev_subdir, "dev",       NULL);
            rpt_subdir_attr_text(d1, dir_devices_i2cN, "i2c-dev", i2c_dev_subdir, "name",      NULL);
            rpt_attr_realpath(   d1, dir_devices_i2cN, "i2c-dev", i2c_dev_subdir, "device",    NULL);
            rpt_attr_realpath(   d1, dir_devices_i2cN, "i2c-dev", i2c_dev_subdir, "subsystem", NULL);


#ifdef old
            g_snprintf(pb2, PATH_MAX, "%s/i2c-dev/%s/dev", dir_devices_i2cN, i2c_dev_subdir);
            value = read_sysfs_attr0(pb2, true);
            rpt_vstring(d1, "%-*s :  %s", offset, pb2, value);

            g_snprintf(pb2, PATH_MAX, "%s/i2c-dev/%s/name", dir_devices_i2cN, i2c_dev_subdir);
            value = read_sysfs_attr0(pb2, true);
            rpt_vstring(d1, "%-*s :  %s", offset, pb2, value);

            g_snprintf(pb1, PATH_MAX, "%s/i2c-dev/%s/device", dir_devices_i2cN, i2c_dev_subdir);
            rpt_vstring(d1, "%-*s -> %s", offset, pb1, realpath(pb1, pb2));

            g_snprintf(pb1, PATH_MAX, "%s/i2c-dev/%s/subsystem", dir_devices_i2cN, i2c_dev_subdir);
            rpt_vstring(d1, "%-*s -> %s", offset, pb1, realpath(pb1, pb2));
#endif
         }
      }
    }
    else {   // device/class not found
       char * s = NULL;
       g_snprintf(pb1, PATH_MAX, "%s/device/class", dir_devices_i2cN);
       rpt_vstring(d1, "%-*s :  Not found.  May be display port", offset, pb1);

       // sys/bus/i2c/devices/i2c-N
       //       device (link)   ../../cardX-DP-X
       //       i2c-dev (dir)
       //       name
       //       subsystem (link, use basename)

#ifdef ALREADY_SHOWN
       g_snprintf(pb1, PATH_MAX, "%s/device", dir_devices_i2cN);
       rpt_vstring(d1, "%-*s -> %s", offset, pb1, realpath(pb1, pb2));

       g_snprintf(pb1, PATH_MAX, "%s/name", dir_devices_i2cN);
       char * val = read_sysfs_attr0(pb1, true);
       rpt_vstring(d1, "%-*s:  %s", offset, pb1, val);
       free(val);
#endif

       g_snprintf(pb1, PATH_MAX, "%s/subsystem", dir_devices_i2cN);
       rpt_vstring(d1, "%-*s -> %s", offset, pb1, realpath(pb1, pb2));

       // /sys/bus/i2c/devices/i2c-N/device:
       //       ddc (link)
       //       device (link)
       //       drm_dp_auxM (dir)
       //       edid
       //       i2c-N (dir)
       //       status
       //       subsysten (link)

       g_snprintf(pb1, PATH_MAX, "%s/device/ddc", dir_devices_i2cN);
       rpt_vstring(d1, "%-*s -> %s", offset, pb1, realpath(pb1, pb2));

        g_snprintf(pb1, PATH_MAX, "%s/device/device", dir_devices_i2cN);
       rpt_vstring(d1, "%-*s -> %s", offset, pb1, realpath(pb1, pb2));

       g_snprintf(pb1, PATH_MAX, "%s/device", dir_devices_i2cN);
       g_snprintf(pb1, PATH_MAX, "%s/device/edid", dir_devices_i2cN);
       GByteArray * edid = read_binary_file(pb1, 256, true);
       rpt_vstring(d1, "%-*s :  %sfound", offset, pb1, (edid) ? "" : "NOT ");
       g_byte_array_free(edid, true);

       g_snprintf(pb1, PATH_MAX, "%s/device/status", dir_devices_i2cN);
       char * val = read_sysfs_attr0(pb1, true);
       rpt_vstring(d1, "%-*s =  %s", offset, pb1, val);
       free(val);

       g_snprintf(pb1, PATH_MAX, "%s/device/subsystem", dir_devices_i2cN);
       rpt_vstring(d1, "%-*s -> %s", offset, pb1, realpath(pb1, pb2));

       // /sys/bus/i2c/devices/i2c-N/device/ddc:
       //      device (link)
       //      i2c-dev (dir)
       //      name
       //      subsystem (link)

       g_snprintf(pb1, PATH_MAX, "%s/device/ddc/device", dir_devices_i2cN);
       rpt_vstring(d1, "%-*s -> %s", offset, pb1, realpath(pb1, pb2));

       g_snprintf(pb1, PATH_MAX, "%s/device/ddc/name", dir_devices_i2cN);
       val = read_sysfs_attr0(pb1, true);
       rpt_vstring(d1, "%-*s =  %s", offset, pb1, val);
       free(val);

       g_snprintf(pb1, PATH_MAX, "%s/device/ddc/subsystem", dir_devices_i2cN);
       rpt_vstring(d1, "%-*s -> %s", offset, pb1, realpath(pb1, pb2));

       g_snprintf(pb1, PATH_MAX, "%s/device/ddc/device", dir_devices_i2cN);
       rpt_vstring(d1, "%-*s :  Skipping linked directory", offset, pb1);

       g_snprintf(pb1, PATH_MAX, "%s/device/ddc/i2c-dev", dir_devices_i2cN);
       char * i2c_dev_subdir = get_single_subdir_name(pb1, NULL, NULL);
       rpt_vstring(d1, "%-*s :  subdirectory = %s", offset, pb1, i2c_dev_subdir);

       // /sys/bus/i2c/devices/i2c-N/device/ddc/i2c-dev/i2c-M
       //       dev
       //       device (link)
       //       name
       //       subsystem (link)

       g_snprintf(pb1, PATH_MAX, "%s/device/ddc/i2c-dev/%s/dev", dir_devices_i2cN, i2c_dev_subdir);
       val = read_sysfs_attr0(pb1, true);
       rpt_vstring(d1, "%-*s =  %s", offset, pb1, val);
       free(val);

       g_snprintf(pb1, PATH_MAX, "%s/device/ddc/i2c-dev/%s/device", dir_devices_i2cN, i2c_dev_subdir);
       rpt_vstring(d1, "%-*s -> %s", offset, pb1, realpath(pb1, pb2));

       g_snprintf(pb1, PATH_MAX, "%s/device/ddc/i2c-dev/%s/name", dir_devices_i2cN, i2c_dev_subdir);
       val = read_sysfs_attr0(pb1, true);
       rpt_vstring(d1, "%-*s =  %s", offset, pb1, val);
       free(val);

       g_snprintf(pb1, PATH_MAX, "%s/device/ddc/i2c-dev/%s/subsystem", dir_devices_i2cN, i2c_dev_subdir);
       rpt_vstring(d1, "%-*s -> %s", offset, pb1, realpath(pb1, pb2));

       g_snprintf(pb1, PATH_MAX, "%s/device/ddc/i2c-dev/%s/name", dir_devices_i2cN, i2c_dev_subdir);
       val = read_sysfs_attr0(pb1, true);
       rpt_vstring(d1, "%-*s =  %s", offset, pb1, val);
       free(val);

       //  /sys/bus/i2c/devices/i2c-N/device/drm_dp_auxN

       g_snprintf(pb1, PATH_MAX, "%s/device", dir_devices_i2cN);
       char * drm_dp_aux_subdir = get_single_subdir_name(pb1, str_starts_with, "drm_dp_aux");
       if (drm_dp_aux_subdir) {
          g_snprintf(pb2, PATH_MAX, "%s/%s", pb1, drm_dp_aux_subdir);
          rpt_vstring(d1, "%-*s :  Found", offset, pb2);
       }
       else {
          g_snprintf(pb2, PATH_MAX, "%s/drm_dp_aux*", pb1);
          rpt_vstring(d1, "%-*s :  Not found", offset, pb2);
       }
       free(s);


       g_snprintf(pb1, PATH_MAX, "%s/device/ddc/device/driver", dir_devices_i2cN);
       // TODO: check that directory exists
       char * fq_driver_path =  realpath(pb1, NULL);
       rpt_vstring(d1, "%-*s -> %s",  offset, pb1, basename(fq_driver_path) );
       free(fq_driver_path);


       g_snprintf(pb1, PATH_MAX, "%s/device/enabled", dir_devices_i2cN);
       char * enabled = read_sysfs_attr0(pb1, true);
       rpt_vstring(d1, "%-*s =  %s", offset, pb1, "enabled", enabled);
       free(enabled);

       if (drm_dp_aux_subdir) {
          g_snprintf(pb1, PATH_MAX, "%s/device/%s/dev", dir_devices_i2cN, drm_dp_aux_subdir);
          val = read_sysfs_attr0(pb1, true);
          rpt_vstring(d1, "%-*s =  %s", offset, pb1, val);
          free(val);

          g_snprintf(pb1, PATH_MAX, "%s/device/%s/device", dir_devices_i2cN, drm_dp_aux_subdir);
          rpt_vstring(d1, "%-*s -> %s", offset, pb1, realpath(pb1, pb2));

          g_snprintf(pb1, PATH_MAX, "%s/device/%s/name", dir_devices_i2cN, drm_dp_aux_subdir);
          val = read_sysfs_attr0(pb1, true);
          rpt_vstring(d1, "%-*s =  %s", offset, pb1, val);
          free(val);

          g_snprintf(pb1, PATH_MAX, "%s/device/%s/device/subsystem", dir_devices_i2cN, drm_dp_aux_subdir);
          rpt_vstring(d1, "%-*s -> %s", offset, pb1, realpath(pb1, pb2));
       }
    }
}




int i2c_compare(const void * v1, const void * v2) {
   char ** s1 = (char**) v1;
   char ** s2 = (char**) v2;
   int result = 0;
   int i1 = i2c_name_to_busno(*s1);
   int i2 = i2c_name_to_busno(*s2);
   if (i1 >= 0 && i2 >= 0) {
      if (i1 < i2)
         result = -1;
      else if (i1 == i2)
         result = 0;
      else
         result = 1;
   }
   else if (i1 >= 0)
      result = -1;
   else if (i2 >= 0)
      result = 1;
   else
      result = strcmp(*s1, *s2);
   return result;
}


void each_i2c_device_new(char * dirname, char * fn, void * accumulator, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, fn=%s", dirname, fn);
   rpt_nl();
   int busno = i2c_name_to_busno(fn);
   if (busno < 0)
      rpt_vstring(1, "Unexpected I2C device name: %s", fn);
   else {
      one_bus_i2c_device(busno, NULL, 1);
   }
}


void dump_sysfs_i2c() {
   rpt_nl();
   rpt_label(0, "Dumping sysfs i2c entries");

   rpt_label(0, "Top level i2c related nodes...");
   rpt_nl();

   char * cmds[] = {
   "ls -l /sys/dev/char/89:*",
   "ls -l /sys/dev/char/237:*",
   "ls -l /sys/bus/i2c/devices",
   "ls -l /sys/devices/pci*",
   "ls -l /sys/class/i2c*",
   NULL
   };

   int ndx = 0;
   while ( cmds[ndx] ) {
      rpt_vstring(1, "%s ...", cmds[ndx]);
      execute_shell_cmd_rpt(cmds[ndx], 2);
      ndx++;
      rpt_nl();
   }

   rpt_label(0, "Detail for /sys/bus/i2c/devices");
   dir_ordered_foreach(
         "/sys/bus/i2c/devices",
         NULL,                 // fn_filter
         i2c_compare,
         each_i2c_device_new,
         NULL,                 // accumulator
         0);                   // depth
}


