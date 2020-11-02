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

#include "i2c/i2c_sysfs.h"

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
      const char * dirname,     // always /sys/bus/i2c/devices
      const char * fn,          // i2c-0, i2c-1, ...
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
   if (busno >= 0) {
      bva_append(accum->sys_bus_i2c_device_numbers, busno);
   }
   else {
      rpt_vstring(depth, "%-34s Unexpected file name: %s", cur_dir_name, fn);
      // DBGMSG("Unexpected /sys/bus/i2c/devices file name: %s", fn);
   }

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
      dir_ordered_foreach(dname, NULL, i2c_compare, each_i2c_device, accumulator, 1);
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


//
// Functions for probing /sys
//



#ifdef OLD
// *** Detail for /sys/bus/i2c/devices (Initial Version)  ***

void one_bus_i2c_device(int busno, void * accumulator, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting. busno=%d", busno);

   int d1 = depth+1;
   char pb1[PATH_MAX];
   // char pb2[PATH_MAX];

   char * dir_devices_i2cN = g_strdup_printf("/sys/bus/i2c/devices/i2c-%d", busno);
   char * real_device_dir = realpath(dir_devices_i2cN, pb1);
   rpt_vstring(depth, "Examining %s -> %s", dir_devices_i2cN, real_device_dir);
   RPT2_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device");
   RPT2_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "name");
   char * device_class = NULL;
   if ( RPT2_ATTR_TEXT(d1, &device_class, dir_devices_i2cN, "device/class") ) {
      if ( str_starts_with(device_class, "0x03") ){   // TODO: replace test
         RPT2_ATTR_TEXT(       d1, NULL, dir_devices_i2cN, "device/boot_vga");
         RPT2_ATTR_REALPATH_BASENAME( d1, NULL, dir_devices_i2cN, "device/driver");
         RPT2_ATTR_REALPATH_BASENAME( d1, NULL, dir_devices_i2cN, "device/driver/module");
         RPT2_ATTR_TEXT(       d1, NULL, dir_devices_i2cN, "device/enable");
         RPT2_ATTR_TEXT(       d1, NULL, dir_devices_i2cN, "device/modalias");
         RPT2_ATTR_TEXT(       d1, NULL, dir_devices_i2cN, "device/vendor");
         RPT2_ATTR_TEXT(       d1, NULL, dir_devices_i2cN, "device/device");
         RPT2_ATTR_TEXT(       d1, NULL, dir_devices_i2cN, "device/subsystem_vendor");
         RPT2_ATTR_TEXT(       d1, NULL, dir_devices_i2cN, "device/subsystem_device");
         char * i2c_dev_subdir = NULL;
         RPT2_ATTR_SINGLE_SUBDIR(d1, &i2c_dev_subdir,  NULL, NULL, dir_devices_i2cN, "i2c-dev");
         if (i2c_dev_subdir) {
            RPT2_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "i2c-dev", i2c_dev_subdir, "dev");
            RPT2_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "i2c-dev", i2c_dev_subdir, "name");
            RPT2_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "i2c-dev", i2c_dev_subdir, "device");
            RPT2_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "i2c-dev", i2c_dev_subdir, "subsystem");
         }
      }
    }
    else {   // device/class not found
       g_snprintf(pb1, PATH_MAX, "%s/%s", dir_devices_i2cN, "device/class");
       rpt_attr_output(d1, pb1, ":", "May be display port");
       RPT2_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "subsystem");
       RPT2_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device/ddc");
       RPT2_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device/device");
       RPT2_ATTR_EDID(    d1, NULL, dir_devices_i2cN, "device/edid");
       RPT2_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "device/status");
       RPT2_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device/subsystem");

       char * realpath = NULL;
       RPT2_ATTR_REALPATH(d1, &realpath, dir_devices_i2cN, "device/device");
       if (realpath) {
          rpt_attr_output(d1, "","","Skipping linked directory");
          free(realpath);
       }

       RPT2_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "device/ddc/name");
       RPT2_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device/ddc/subsystem");

       char * i2c_dev_subdir = NULL;
       RPT2_ATTR_SINGLE_SUBDIR(d1, &i2c_dev_subdir, NULL, NULL, dir_devices_i2cN, "device/ddc/i2c-dev");

       // /sys/bus/i2c/devices/i2c-N/device/ddc/i2c-dev/i2c-M
       //       dev
       //       device (link)
       //       name
       //       subsystem (link)

       RPT2_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "device/ddc/i2c-dev", i2c_dev_subdir, "dev");
       RPT2_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device/ddc/i2c-dev", i2c_dev_subdir, "device");
       RPT2_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "device/ddc/i2c-dev", i2c_dev_subdir, "name");
       RPT2_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device/ddc/i2c-dev", i2c_dev_subdir, "subsystem");

       //  /sys/bus/i2c/devices/i2c-N/device/drm_dp_auxN

       char * drm_dp_aux_subdir = NULL;
       RPT2_ATTR_SINGLE_SUBDIR(    d1, &drm_dp_aux_subdir,  str_starts_with, "drm_dp_aux", dir_devices_i2cN, "device");
       RPT2_ATTR_REALPATH_BASENAME(d1, NULL, dir_devices_i2cN, "device/ddc/device/driver");
       RPT2_ATTR_TEXT(             d1, NULL, dir_devices_i2cN, "device/enabled");

       if (drm_dp_aux_subdir) {
          RPT2_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "device", drm_dp_aux_subdir, "dev");
          RPT2_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device", drm_dp_aux_subdir, "device");
          RPT2_ATTR_TEXT(    d1, NULL, dir_devices_i2cN, "device", drm_dp_aux_subdir, "name");
          RPT2_ATTR_REALPATH(d1, NULL, dir_devices_i2cN, "device", drm_dp_aux_subdir, "device/subsystem");
       }
    }
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

// *** Detail for /sys/class/drm (initial version) ***

void each_drm_device(char * dirname, char * fn, void * accumulator, int depth) {
   bool debug = false;
   DBGMSF(debug, "Starting. dirname=%s, fn=%s", dirname, fn);
   rpt_nl();
   int d1 = depth+1;

   char * drm_cardX_dir = g_strdup_printf("/sys/class/drm/%s", fn);
   char * real_cardX_dir = realpath(drm_cardX_dir, NULL);
   rpt_vstring(depth, "Examining %s -> %s", drm_cardX_dir, real_cardX_dir);

   DBGMSF(debug, "Wolf 11");
   // e.g. /sys/class/drm/card0-DP-1
   RPT2_ATTR_REALPATH(     d1, NULL, drm_cardX_dir, "ddc");
   RPT2_ATTR_REALPATH(     d1, NULL, drm_cardX_dir, "device");
   char * drm_dp_aux_subdir = NULL;   // exists only if DP
   RPT2_ATTR_SINGLE_SUBDIR(d1, &drm_dp_aux_subdir, str_starts_with, "drm_dp_aux", drm_cardX_dir);
   RPT2_ATTR_EDID(         d1, NULL, drm_cardX_dir, "edid");
   RPT2_ATTR_TEXT(         d1, NULL, drm_cardX_dir, "enabled");
   char * i2cN_subdir = NULL;  // exists only if DP
   RPT2_ATTR_SINGLE_SUBDIR(d1, &i2cN_subdir, str_starts_with, "i2c-", drm_cardX_dir);
   RPT2_ATTR_TEXT(         d1, NULL, drm_cardX_dir, "status");
   RPT2_ATTR_REALPATH(     d1, NULL, drm_cardX_dir, "subsystem");

   // messages subdirectories of card0/DP-1
   // e.g. /sys/class/drm/card0-DP-1/drm_dp_aux0
   //      does not exist for non-DP
   DBGMSF(debug, "Wolf 12");
   if (drm_dp_aux_subdir) {
      rpt_nl();
      RPT2_ATTR_TEXT(         d1, NULL, drm_cardX_dir, drm_dp_aux_subdir, "dev");
      RPT2_ATTR_REALPATH(     d1, NULL, drm_cardX_dir, drm_dp_aux_subdir, "device");
      RPT2_ATTR_TEXT(         d1, NULL, drm_cardX_dir, drm_dp_aux_subdir, "name");
   // RPT2_ATTR_REALPATH(     d1, NULL, drm_cardX_dir, drm_dp_aux_subdir, "subsystem")
   }

   // e.g. /sys/class/drm/card0-DP-1/i2c-13
   // does not exist for non-DP


   DBGMSF(debug, "Wolf 13");
   if (i2cN_subdir) {
      rpt_nl();
      RPT2_ATTR_REALPATH(     d1, NULL, drm_cardX_dir, i2cN_subdir, "device");
      RPT2_ATTR_NOTE_SUBDIR(  d1, NULL, drm_cardX_dir, i2cN_subdir, "i2c-dev");
      RPT2_ATTR_TEXT(         d1, NULL, drm_cardX_dir, i2cN_subdir, "name");
      RPT2_ATTR_REALPATH(     d1, NULL, drm_cardX_dir, i2cN_subdir, "subsystem");

      rpt_nl();
      // e.g. /sys/class/drm-card0-DP-1/i2c-13/i2c-dev
      RPT2_ATTR_NOTE_SUBDIR(  d1, NULL, drm_cardX_dir, i2cN_subdir, "i2c-dev", i2cN_subdir);   // or can subdir name vary?

      // e.g. /sys/class/drm-card0-DP-1/i2c-13/i2c-dev/i2c-13
      RPT2_ATTR_TEXT(        d1, NULL, drm_cardX_dir, i2cN_subdir, "i2c-dev", i2cN_subdir, "dev");
      RPT2_ATTR_REALPATH(    d1, NULL, drm_cardX_dir, i2cN_subdir, "i2c-dev", i2cN_subdir, "device");
      RPT2_ATTR_TEXT(        d1, NULL, drm_cardX_dir, i2cN_subdir, "i2c-dev", i2cN_subdir, "name");
      RPT2_ATTR_REALPATH(    d1, NULL, drm_cardX_dir, i2cN_subdir, "i2c-dev", i2cN_subdir, "subsystem");
   }

   free(real_cardX_dir);
}
#endif


bool drm_filter(char * name) {
   return str_starts_with(name, "card") && strlen(name) > 5;
}


bool predicate_cardN(const char * val) {
   return str_starts_with(val, "card");
}


void sysfs_dir_cardN_cardNconnector(
      const char * dirname,
      const char * filename,
      void *       accumulator,
      int          depth)
{
   char dirname_fn[PATH_MAX];
   g_snprintf(dirname_fn, PATH_MAX, "%s/%s", dirname, filename);
   // DBGMSG("dirname=%s, filename=%s, dirname_fn=%s", dirname, filename, dirname_fn);
   int d0 = depth;
   // int d1 = depth+1;
   // int d2 = depth+2;

   RPT2_ATTR_REALPATH( d0, NULL, dirname_fn, "device");
   RPT2_ATTR_REALPATH( d0, NULL, dirname_fn, "ddc");
   RPT2_ATTR_EDID(     d0, NULL, dirname_fn, "edid");
   RPT2_ATTR_TEXT(     d0, NULL, dirname_fn, "enabled");
   RPT2_ATTR_TEXT(     d0, NULL, dirname_fn, "status");
   RPT2_ATTR_REALPATH( d0, NULL, dirname_fn, "subsystem");

   // for DP, also:
   //    drm_dp_auxN
   //    i2c-N

   char * dir_drm_dp_aux = NULL;
   RPT2_ATTR_SINGLE_SUBDIR(d0, &dir_drm_dp_aux, str_starts_with, "drm_dp_aux", dirname_fn);
   if (dir_drm_dp_aux) {
      RPT2_ATTR_REALPATH(d0, NULL, dirname_fn, dir_drm_dp_aux, "device");
      RPT2_ATTR_TEXT(    d0, NULL, dirname_fn, dir_drm_dp_aux, "dev");
      RPT2_ATTR_TEXT(    d0, NULL, dirname_fn, dir_drm_dp_aux, "name");
      RPT2_ATTR_REALPATH(d0, NULL, dirname_fn, dir_drm_dp_aux, "subsystem");
   }
   char * dir_i2cN = NULL;
   RPT2_ATTR_SINGLE_SUBDIR(d0, &dir_i2cN, str_starts_with, "i2c-",dirname_fn);
   if (dir_i2cN) {
      char pb1[PATH_MAX];
      g_snprintf(pb1, PATH_MAX, "%s/%s", dirname_fn, dir_i2cN);
      // sysfs_dir_i2c_dev(fqfn, dir_i2cN, accumulator, d0);
      char * dir_i2cN_i2cdev_i2cN = NULL;
      RPT2_ATTR_SINGLE_SUBDIR(d0, &dir_i2cN_i2cdev_i2cN, str_starts_with, "i2c-", dirname_fn, dir_i2cN, "i2c-dev");
      if (dir_i2cN_i2cdev_i2cN) {
         RPT2_ATTR_REALPATH(d0, NULL, dirname_fn, dir_i2cN, "i2c-dev", dir_i2cN_i2cdev_i2cN, "device");
         RPT2_ATTR_TEXT(    d0, NULL, dirname_fn, dir_i2cN, "i2c-dev", dir_i2cN_i2cdev_i2cN, "dev");
         RPT2_ATTR_TEXT(    d0, NULL, dirname_fn, dir_i2cN, "i2c-dev", dir_i2cN_i2cdev_i2cN, "name");
         RPT2_ATTR_REALPATH(d0, NULL, dirname_fn, dir_i2cN, "i2c-dev", dir_i2cN_i2cdev_i2cN, "subsystem");
      }
      RPT2_ATTR_REALPATH( d0, NULL, dirname_fn, dir_i2cN, "device");
      RPT2_ATTR_TEXT(     d0, NULL, dirname_fn, dir_i2cN, "name");
      RPT2_ATTR_REALPATH( d0, NULL, dirname_fn, dir_i2cN, "subsystem");
   }
}


/**  Process all /sys/bus/pci/devices/<pci-device>/cardN directories
 *
 *  These directories exist for DisplayPort connectors
 *
 *  Note the realpath for these directories is one of
 *          /sys/bus/devices/NNNN:NN:NN.N/cardN
 *          /sys/bus/devices/NNNN:NN:NN.N/NNNN:NN:nn.N/cardN
 *  Include subdirectory i2c-dev/i2c-N
 *
 *  @param dirname      name of device directory
 *  @param filename     i2c-N
 *  @param accumulator
 *  @param depth        logical indentation depth
 */

void sysfs_dir_cardN(
      const char * dirname,
      const char * filename,
      void *       accumulator,
      int          depth)
{
   char fqfn[PATH_MAX];
   g_snprintf(fqfn, PATH_MAX, "%s/%s", dirname, filename);

   dir_foreach(fqfn, predicate_cardN, sysfs_dir_cardN_cardNconnector, accumulator, depth);
}


/**  Process /sys/bus/pci/devices/<pci-device>/i2c-N directory
 *
 *  These directories exist for non-DP connectors
 *
 *  Note the realpath for these directories is one of
 *          /sys/bus/devices/NNNN:NN:NN.N/i2c-N
 *          /sys/bus/devices/NNNN:NN:NN.N/NNNN:NN:nn.N/i2c-N
 *  Include subdirectory i2c-dev/i2c-N
 *
 *  @param dirname      name of device directory
 *  @param filename     i2c-N
 *  @param accumulator
 *  @param depth        logical indentation depth
 */
void sysfs_dir_i2cN(
      const char * dirname,
      const char * filename,
      void *       accumulator,
      int          depth)
{
   char fqfn[PATH_MAX];
   g_snprintf(fqfn, PATH_MAX, "%s/%s", dirname, filename);
   int d0 = depth;

   RPT2_ATTR_REALPATH(d0,  NULL,     fqfn, "device");
   RPT2_ATTR_TEXT(    d0,  NULL,     fqfn, "name");
   RPT2_ATTR_REALPATH(d0,  NULL,     fqfn, "subsystem");
   char * i2c_dev_fn = NULL;
   RPT2_ATTR_SINGLE_SUBDIR(d0, &i2c_dev_fn, streq, "i2c-dev", fqfn);
   if (i2c_dev_fn) {
      char * i2cN = NULL;
      RPT2_ATTR_SINGLE_SUBDIR(d0, &i2cN,NULL, NULL, fqfn, "i2c-dev");
      RPT2_ATTR_REALPATH(     d0, NULL, fqfn, "i2c-dev", i2cN, "device");
      RPT2_ATTR_TEXT(         d0, NULL, fqfn, "i2c-dev", i2cN, "dev");
      RPT2_ATTR_TEXT(         d0, NULL, fqfn, "i2c-dev", i2cN, "name");
      RPT2_ATTR_REALPATH(     d0, NULL, fqfn, "i2c-dev", i2cN, "subsystem");
   }
}


bool startswith_i2c(const char * value) {
   return str_starts_with(value, "i2c-");
}


bool class_display_device_predicate(char * value) {
   return str_starts_with(value, "0x03");
}


/**  Process a single /sys/bus/pci/devices/<pci-device>
 *
 *   Returns immediately if class is not a display device or docking station
 *
 *   PPPP:BB:DD:F
 *      PPPP     PCI domain
 *      BB       bus number
 *      DD       device number
 *      F        device function
 *
 *   Note the realpath for these directories is one of
 *          /sys/bus/devices/PPPP:BB:DD.F
 *          /sys/bus/devices/NNNN:NN:NN.N/PPPP:BB:DD:F
 */
void one_pci_device(
      const char * dirname,
      const char * filename,
      void *       accumulator,
      int          depth)
{
   int d0 = depth;
   int d1 = depth+1;

   char dir_fn[PATH_MAX];
   g_snprintf(dir_fn, PATH_MAX, "%s/%s", dirname, filename);

   char * device_class = read_sysfs_attr(dir_fn, "class", false);
   if (!device_class)
      return;
   unsigned class_id = h2uint(device_class);
   // DBGMSF(debug, "class_id: 0x%08x", class_id);
   //   if (str_starts_with(device_class, "0x03")) {
   if (class_id >> 16 != 0x03 &&     // Display controller
       class_id >> 16 != 0x0a)       // Docking station
   {
       return;
   }
   free(device_class);

   char rpath[PATH_MAX];
   // without assignment, get warning that return value of realpath() is not used
   // causes compilation failures since all warnings treated as errors
   _Pragma("GCC diagnostic push")
   _Pragma("GCC diagnostic ignored \"-Wunused-result\"")
   realpath(dir_fn, rpath);
   _Pragma("GCC diagnostic pop")

   // DBGMSG("dirname=%s, filename=%s, pb1=%s, rpath=%s", dirname, filename, pb1, rpath);
   rpt_nl();
    rpt_vstring(       d0, "Examining %s/%s -> %s", dirname, filename, rpath);
    RPT2_ATTR_REALPATH(d1, NULL, dirname, filename, "device");
    RPT2_ATTR_TEXT(    d1, NULL, dirname, filename, "class");
    RPT2_ATTR_TEXT(    d1, NULL, dirname, filename, "boot_vga");
    RPT2_ATTR_REALPATH_BASENAME(d1, NULL, dirname, filename, "driver");
    RPT2_ATTR_TEXT(    d1, NULL, dirname, filename, "enable");
    RPT2_ATTR_TEXT(    d1, NULL, dirname, filename, "modalias");
    RPT2_ATTR_TEXT(    d1, NULL, dirname, filename, "vendor");
    RPT2_ATTR_TEXT(    d1, NULL, dirname, filename, "device");
    RPT2_ATTR_TEXT(    d1, NULL, dirname, filename, "subsystem_vendor");
    RPT2_ATTR_TEXT(    d1, NULL, dirname, filename, "subsystem_device");
    RPT2_ATTR_REALPATH(d1, NULL, dirname, filename, "subsystem");

    // Process drm subdirectory
    char * drm_fn = NULL;
    bool has_drm_dir = RPT2_ATTR_SINGLE_SUBDIR(d1, &drm_fn, streq, "drm", dir_fn);
    if (has_drm_dir) {
       char dir_fn_drm[PATH_MAX];
       g_snprintf(dir_fn_drm, PATH_MAX, "%s/%s", dir_fn, "drm");
       dir_ordered_foreach(
             dir_fn_drm,
             predicate_cardN,   // only subdirectories named drm/cardN
             gaux_ptr_scomp,    // GCompareFunc
             sysfs_dir_cardN,
             accumulator,
             d1);
    }

    // Process i2c-N subdirectories:
    dir_ordered_foreach(
          dir_fn,
          startswith_i2c,       // only subdirectories named i2c-N
          i2c_compare,          // order by i2c device number, handles names not of form "i2c-N"
          sysfs_dir_i2cN,
          accumulator,
          d1);
}

#ifdef UNUSED
bool startswith_pci(char * value) {
   return str_starts_with(value, "pci");
}
#endif


/** Master function for dumping /sys directory
 */

void dump_sysfs_i2c() {
   rpt_nl();
   rpt_label(0, "Dumping sysfs i2c entries");

   rpt_label(0, "Top level i2c related nodes...");
   rpt_nl();

   char * cmds[] = {
   "ls -l /sys/dev/char/89:*",
   "ls -l /sys/dev/char/237:*",
   "ls -l /sys/dev/char/238:*",
   "ls -l /sys/dev/char/239:*",
   "ls -l /sys/bus/i2c/devices",
   "ls -l /sys/bus/pci/devices",
   "ls -l /sys/bus/pci_express/devices",
   "ls -l /sys/bus/platform/devices",   // not symbolic links
   "ls -l /sys/devices/pci*",
   "ls -l /sys/class/drm*",             // drm, drm_dp_aux_dev
   "ls -l /sys/class/i2c*",             // i2c, i2c-adapter, i2c-dev
   "ls -l /sys/class/pci*",             // pci_bus, pci_epc
   NULL
   };

   int ndx = 0;
   while ( cmds[ndx] ) {
      rpt_vstring(1, "%s ...", cmds[ndx]);
      execute_shell_cmd_rpt(cmds[ndx], 2);
      ndx++;
      rpt_nl();
   }

#ifdef OLD
   rpt_nl();
   rpt_label(0, "*** Detail for /sys/bus/i2c/devices (Initial Version) ***");
   dir_ordered_foreach(
         "/sys/bus/i2c/devices",
         NULL,                 // fn_filter
         i2c_compare,
         each_i2c_device_new,
         NULL,                 // accumulator
         0);                   // depth

   rpt_nl();
   rpt_label(0, "*** Detail for /sys/class/drm  (Initial Version) ***");
   dir_ordered_foreach(
         "/sys/class/drm",
         drm_filter                ,
         gaux_ptr_scomp,    // GCompareFunc
         each_drm_device,    //
         NULL,                 // accumulator
         0);                   // depth
#endif

#ifdef TMI
   GPtrArray *  video_devices =   execute_shell_cmd_collect(
         "find /sys/devices -name class | xargs grep -il 0x03 | xargs dirname | xargs ls -lR");
   rpt_nl();

   rpt_vstring(0, "Display devices: (class 0x03nnnn)");
   for (int ndx = 0; ndx < video_devices->len; ndx++) {
      char * dirname = g_ptr_array_index(video_devices, ndx);
      rpt_vstring(2, "%s", dirname);
   }
#endif

   rpt_nl();
   rpt_vstring(0, "*** Reporting video devices ***");
   // rpt_vstring(0, "using dir_foreach");
   // general probe
   dir_foreach(
            "/sys/bus/pci/devices",
            NULL,
            one_pci_device,
            NULL,
            0);
}

