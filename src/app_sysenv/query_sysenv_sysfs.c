/* query_sysenv_sysfs.c
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

#define _GNU_SOURCE

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>

#include "util/data_structures.h"
#include "util/device_id_util.h"
#include "util/file_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"
#include "util/sysfs_util.h"

#include "base/core.h"
#include "base/linux_errno.h"

#include "query_sysenv_base.h"
#include "query_sysenv_xref.h"

#include "query_sysenv_sysfs.h"


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


/** Gets the sysfs name of an I2C device,
 *  i.e. the value of /sys/bus/in2c/devices/i2c-n/name
 *
 *  \param  busno   I2C bus number
 *  \return newly allocated string containing attribute value,
 *          NULL if not found
 *
 *  \remark
 *  Caller is responsible for freeing returned value
 */
char * get_i2c_device_sysfs_name(int busno) {
   char workbuf[50];
   snprintf(workbuf, 50, "/sys/bus/i2c/devices/i2c-%d/name", busno);
   char * name = file_get_first_line(workbuf, /*verbose */ false);
   // DBGMSG("busno=%d, returning: %s", busno, bool_repr(result));
   return name;
}

#ifdef UNUSED
static bool is_smbus_device_using_sysfs(int busno) {
#ifdef OLD
   char workbuf[50];
   snprintf(workbuf, 50, "/sys/bus/i2c/devices/i2c-%d/name", busno);
   char * name = file_get_first_line(workbuf, /*verbose */ false);
#endif
   char * name = get_i2c_device_sysfs_name(busno);

   bool result = false;
   if (name && str_starts_with(name, "SMBus"))
      result = true;
   free(name);
   // DBGMSG("busno=%d, returning: %s", busno, bool_repr(result));
   return result;
}
#endif




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


   // printf("vendor: %s\n", read_sysfs_attr(cur_dir_name, "vendor", true));
   // printf("device: %s\n", read_sysfs_attr(cur_dir_name, "device", true));
   // printf("subsystem_device: %s\n", read_sysfs_attr(cur_dir_name, "subsystem_device", true));
   // printf("subsystem_vendor: %s\n", read_sysfs_attr(cur_dir_name, "subsystem_vendor", true));

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


/** Reads the device identifiers from directory
 *  /sys/bus/pci/devices/nnnn:nn:nn.n/ by reading and parsing the modalias
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

   rpt_vstring(0, "Reading device ids by parsing modalias attribute...");
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






void lspci_alt_one_i2c(char * dirname, char * fn, void * accumulator, int depth) {
   if (str_starts_with(fn, "i2c")) {
      char cur_dir[PATH_MAX];
      snprintf(cur_dir, PATH_MAX, "%s/%s", dirname, fn);
      char * name = read_sysfs_attr_w_default(cur_dir, "name","", false);
      rpt_vstring(depth, "I2C device: %-10s name: %s", fn, name);
   }
}







// n. equivalent to examining
//  /sys/bus/pci/devices/0000:nn:nn.n/
//        boot_vga   1  if the boot device, appears not exist ow
//        class      0x030000 for video
//        device     hex PID
//        driver    -> /sys/bus/pci/drivers/radeon
//        drm
//           card0 (dir0
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



// sprintf(cur_dir_name, "%s/%s", dirname, dent->d_name);

// typedef void (*Dir_Foreach_Func)(char * fn, void * accumulator);
void lspci_alt_one_device(
      char * dirname,
      char * fn,
      void * accumulator,
      int    depth)
{
   bool debug = false;
   DBGMSF(debug, "Starting.  dirname=%s, fn=%s", dirname, fn);

   int d1 = depth+1;

   char cur_dir_name[PATH_MAX];
   snprintf(cur_dir_name, PATH_MAX, "%s/%s", dirname, fn);
   char *device_class = read_sysfs_attr(cur_dir_name, "class", true);
   if (!device_class) {
      rpt_vstring(depth, "Unexpected for %s: class not found", cur_dir_name);
      goto bye;
   }

   // hack - should convert hex value, use device id table lookup of class name
   // DBGMSG("class = %s", device_class);
   if (str_starts_with(device_class, "0x03")) {
      bool is_primary_video = false;

      if (str_starts_with(device_class, "0x0300")) {
         // DBGMSG("VGA compatible controller");
         is_primary_video = true;
      }
      else    if (str_starts_with(device_class, "0x0380")) {
         // DBGMSG("Other display controller");
      }
      else {
         rpt_vstring(depth, "Unexpected class for video device: %s", device_class);
         goto bye;
      }

      char * boot_vga = read_sysfs_attr_w_default(cur_dir_name, "boot_vga", "-1", false);
      // DBGMSG("boot_vga: %s", boot_vga);
      bool boot_vga_flag = (boot_vga && streq(boot_vga, "1")) ;
      // if (boot_vga_flag) {
      //   DBGMSG("boot_vga set");
      // }
      // DBGMSG("primary video: %s", bool_repr(is_primary_video));

      Device_Ids devids = read_device_ids1(cur_dir_name);

      Pci_Usb_Id_Names devnames =  devid_get_pci_names(
                      devids.vendor_id,
                      devids.device_id,
                      devids.subvendor_id,
                      devids.subdevice_id,
                      4);
      // DBGMSG("vendor: %s, device: %s", devnames.vendor_name, devnames.device_name);

      rpt_vstring(depth, "%s video controller: %s %s (boot_vga is %sset)",
                         (is_primary_video) ? "Primary" : "Secondary",
                         devnames.vendor_name,
                         devnames.device_name,
                         (boot_vga_flag) ? " " : "not " );
      rpt_vstring(d1, "PCI device path: %s", fn);
      char fnbuf[PATH_MAX];
      snprintf(fnbuf, PATH_MAX, "%s/%s", cur_dir_name, "driver");
      // DBGMSG("fnbuf=%s", fnbuf);
      char resolved_path[PATH_MAX];
      char * rp = realpath(fnbuf,resolved_path);
      int errsv = errno;
      // DBGMSG("resolved_path: %s", rp);
      if (!rp) {
         DBGMSG("Unable to resolve driver path. errno = %d", errsv);
         rpt_vstring(d1, "Driver:  none");
      }
      else {
         char * rp2 = strdup(rp);
         char * driver_name = basename(rp2);
         rpt_vstring(d1, "Driver: %s", driver_name);
      }
      dir_foreach(cur_dir_name, NULL, lspci_alt_one_i2c, NULL, d1);


   }

bye:
   return;
}

bool query_card_and_driver_using_lspci_alt() {
   DBGMSG("Starting");

   dir_foreach("/sys/bus/pci/devices", NULL, lspci_alt_one_device, NULL, 1);

   DBGMSG("Done");
   return true;
}




/* Scans /sys/bus/pci/devices for video devices.
 * Reports on the devices, and returns a singly linked list of driver names.
 *
 * Arguments:   none
 *
 * Returns:     singly linked list of video driver names
 */
struct driver_name_node * query_card_and_driver_using_sysfs(Env_Accumulator * accum) {
   rpt_vstring(0,"Obtaining card and driver information from /sys...");

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

   char * driver_name = NULL;
   struct driver_name_node * driver_list = NULL;

   struct dirent *dent;
   DIR           *d;

   if (accum->is_arm) {
      rpt_vstring(0, "Machine architecture is %s.  Skipping /sys/bus/pci checks.", accum->architecture);
      char * platform_drivers_dir_name = "/sys/bus/platform/drivers";
      d = opendir(platform_drivers_dir_name);
      if (!d) {
         rpt_vstring(0,"Unable to open directory %s: %s", platform_drivers_dir_name, strerror(errno));
      }
      else {
         while ((dent = readdir(d)) != NULL) {
            // DBGMSG("%s", dent->d_name);
            char cur_fn[100];
            char cur_dir_name[100];
            if (!streq(dent->d_name, ".") && !streq(dent->d_name, "..") ) {
               // sprintf(cur_dir_name, "%s/%s", pci_devices_dir_name, dent->d_name);
               sprintf(cur_fn, "%s", cur_dir_name);
               if (streq(cur_dir_name, "vc4_v3d")) {
                  char * driver_name = cur_fn;
                  printf(    "   Driver name:    %s\n", driver_name);
                  struct driver_name_node * new_node = calloc(1, sizeof(struct driver_name_node));
                  new_node->driver_name = strdup(driver_name);
                  new_node->next = driver_list;
                  driver_list = new_node;
               }
            }
         }
      }
      closedir(d);
   }
   else {
      char * pci_devices_dir_name = "/sys/bus/pci/devices";
      d = opendir(pci_devices_dir_name);
      if (!d) {
         rpt_vstring(0,"Unable to open directory %s: %s", pci_devices_dir_name, strerror(errno));
      }
      else {
         while ((dent = readdir(d)) != NULL) {
            // DBGMSG("%s", dent->d_name);
            char cur_fn[100];
            char cur_dir_name[100];
            if (!streq(dent->d_name, ".") && !streq(dent->d_name, "..") ) {
               sprintf(cur_dir_name, "%s/%s", pci_devices_dir_name, dent->d_name);
               sprintf(cur_fn, "%s/class", cur_dir_name);
               // read /sys/bus/pci/devices/nnnn:nn:nn.n/class
               char * class_id = read_sysfs_attr(cur_dir_name, "class", /*verbose=*/true);
               // printf("%s: |%s|\n", cur_fn, class_id);
               if (str_starts_with(class_id, "0x03")) {
                  // printf("%s = 0x030000\n", cur_fn);
                  rpt_nl();
                  rpt_vstring(0,"Determining driver name and possibly version...");
                  // DBGMSG("cur_dir_name: %s", cur_dir_name);
                  char workfn[PATH_MAX];
                  sprintf(workfn, "%s/%s", cur_dir_name, "driver");
                  char resolved_path[PATH_MAX];
                  char * rpath = realpath(workfn, resolved_path);
                  if (!rpath) {
                     int errsv = errno;
                     rpt_vstring(0,"Cannot determine driver name");
                     rpt_vstring(0, "realpath(%s) returned NULL, errno=%d (%s)",
                                     workfn, errsv, linux_errno_name(errsv));
                     if (errsv == ENOENT) {
                        // fail in virtual environment?
                        // Raspberry Pi
                        rpt_vstring(0, "Directory not found: %s", cur_dir_name);
                     }
                     else {
                        // rpt_vstring(0, "realpath(%s) returned NULL, errno=%d (%s)",
                        //                 workfn, errsv, linux_errno_name(errsv));
                     }
                  }
                  else {
                     // printf("realpath returned %s\n", rpath);
                     // printf("%s --> %s\n",workfn, resolved_path);
                     char * final_slash_ptr = strrchr(rpath, '/');
                     // TODO: handle case where there are more than 1 video drivers loaded,
                     // say if the system contains both an AMD and Nvidia card
                     driver_name = final_slash_ptr+1;
                     printf(    "   Driver name:    %s\n", driver_name);
                     struct driver_name_node * new_node = calloc(1, sizeof(struct driver_name_node));
                     new_node->driver_name = strdup(driver_name);
                     new_node->next = driver_list;

                     driver_list = new_node;


                     char driver_module_dir[PATH_MAX];
                     sprintf(driver_module_dir, "%s/driver/module", cur_dir_name);
                     // printf("driver_module_dir: %s\n", driver_module_dir);
                     char * driver_version = read_sysfs_attr(driver_module_dir, "version", false);
                     if (driver_version)
                         rpt_vstring(0,"   Driver version: %s", driver_version);
                     else
                        rpt_vstring(0,"   Unable to determine driver version");
                  }

                  rpt_nl();
                  rpt_vstring(0, "Reading device ids from individual attribute files...");
                  Device_Ids dev_ids = read_device_ids1(cur_dir_name);
                  Device_Ids dev_ids2 = read_device_ids2(cur_dir_name);
                  assert(dev_ids.vendor_id == dev_ids2.vendor_id);
                  assert(dev_ids.device_id == dev_ids2.device_id);
                  assert(dev_ids.subvendor_id == dev_ids2.subvendor_id);
                  assert(dev_ids.subdevice_id == dev_ids2.subdevice_id);

                  // printf("\nLooking up names in pci.ids...\n");
                  // rpt_nl();
                  rpt_vstring(0,"Video card identification:");
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

                     rpt_vstring(0,"   Vendor:              %04x       %s", dev_ids.vendor_id, names.vendor_name);
                     rpt_vstring(0,"   Device:              %04x       %s", dev_ids.device_id, names.device_name);
                     if (names.subsys_or_interface_name)
                     rpt_vstring(0,"   Subvendor/Subdevice: %04x/%04x  %s", dev_ids.subvendor_id, dev_ids.subdevice_id, names.subsys_or_interface_name);
                  }
                  else {
                     rpt_vstring(0,"Unable to find pci.ids file for name lookup.");
                     rpt_vstring(0,"   Vendor:              %04x       ", dev_ids.vendor_id);
                     rpt_vstring(0,"   Device:              %04x       ", dev_ids.device_id);
                     rpt_vstring(0,"   Subvendor/Subdevice: %04x/%04x  ", dev_ids.subvendor_id, dev_ids.subdevice_id);
                  }
               }
               else if (str_starts_with(class_id, "0x0a")) {
                  DBGMSG("Encountered docking station (class 0x0a) device. dir=%s", cur_dir_name);
               }
            }
         }
         closedir(d);
      }
   }
   accum->driver_list = driver_list;
   return driver_list;
}


//
// Using sysfs
//


void query_loaded_modules_using_sysfs() {
   rpt_nl();
   rpt_vstring(0,"Testing if modules are loaded using /sys...");
   // known_video_driver_modules
   // other_driver_modules

   char ** pmodule_name = get_known_video_driver_modules();
   char * curmodule;
   int ndx;
   for (ndx=0; (curmodule=pmodule_name[ndx]) != NULL; ndx++) {
      bool is_loaded = is_module_loaded_using_sysfs(curmodule);
      // DBGMSF(debug, "is_loaded=%d", is_loaded);
      rpt_vstring(0,"   Module %-16s is %sloaded", curmodule, (is_loaded) ? "" : "NOT ");
   }
   pmodule_name = get_other_driver_modules();
   for (ndx=0; (curmodule=pmodule_name[ndx]) != NULL; ndx++) {
      bool is_loaded = is_module_loaded_using_sysfs(curmodule);
      rpt_vstring(0,"   Module %-16s is %sloaded", curmodule, (is_loaded) ? "" : "NOT ");
   }
}


void query_i2c_bus_using_sysfs() {
   struct dirent *dent;
   DIR           *d;
   char          *dname;

   rpt_nl();
   rpt_vstring(0,"Examining /sys/bus/i2c/devices...");
   dname = "/sys/bus/i2c";
   d = opendir(dname);
   if (!d) {
      rpt_vstring(1, "i2c bus not defined in sysfs. Unable to open directory %s: %s\n",
                     dname, strerror(errno));
   }
   else {
      closedir(d);
      dname = "/sys/bus/i2c/devices";
      d = opendir(dname);
      if (!d) {
         rpt_vstring(1, "Unable to open sysfs directory %s: %s\n", dname, strerror(errno));
      }
      else {
         bool i2c_seen = false;
         while ((dent = readdir(d)) != NULL) {
            // DBGMSF("%s", dent->d_name);
            // char cur_fn[100];
            char cur_dir_name[100];
            if (!streq(dent->d_name, ".") && !streq(dent->d_name, "..") ) {
               // DBGMSF(debug, "dent->dname: %s", dent->d_name);
               sprintf(cur_dir_name, "%s/%s", dname, dent->d_name);
               char * dev_name = read_sysfs_attr(cur_dir_name, "name", true);
               rpt_vstring(1, "%s/name: %s", cur_dir_name, dev_name);
               i2c_seen = true;
            }
         }
         if (!i2c_seen)
            rpt_vstring(1, "No i2c devices found in %s", dname);
         closedir(d);
      }
   }
}


void query_drm_using_sysfs() {
   struct dirent *dent;
   struct dirent *dent2;
   DIR           *d;
   char          *dname;
   char          dnbuf[90];
   char          cardname[10];

   rpt_nl();
   rpt_vstring(0,"Examining /sys/class/drm...");
   dname = "/sys/class/drm";
   d = opendir(dname);
   if (!d) {
      rpt_vstring(1, "drm not defined in sysfs. Unable to open directory %s: %s\n",
                     dname, strerror(errno));
   }
   else {
      closedir(d);
      int cardno = 0;
      for (;;cardno++) {
         snprintf(cardname, 10, "card%d", cardno);
         snprintf(dnbuf, 80, "/sys/class/drm/%s", cardname);
         d = opendir(dnbuf);
         if (!d) {
            // rpt_vstring(1, "Unable to open sysfs directory %s: %s\n", dnbuf, strerror(errno));
            break;
         }
         else {
            while ((dent = readdir(d)) != NULL) {
               // DBGMSG("%s", dent->d_name);
               // char cur_fn[100];
               if (str_starts_with(dent->d_name, cardname)) {
                  rpt_vstring(1, "Found connector: %s", dent->d_name);
                 char cur_dir_name[100];
                 sprintf(cur_dir_name, "%s/%s", dnbuf, dent->d_name);

                 // char * s_dpms = read_sysfs_attr(cur_dir_name, "dpms", false);
                 // rpt_vstring(1, "%s/dpms: %s", cur_dir_name, s_dpms);

                 // char * s_enabled = read_sysfs_attr(cur_dir_name, "enabled", false);
                 //  rpt_vstring(1, "%s/enabled: %s", cur_dir_name, s_enabled);

                 char * s_status = read_sysfs_attr(cur_dir_name, "status", false);
                 rpt_vstring(2, "%s/status: %s", cur_dir_name, s_status);
                 // edid present iff status == "connected"
                 if (streq(s_status, "connected")) {
                    GByteArray * gba_edid = read_binary_sysfs_attr(
                          cur_dir_name, "edid", 128, /*verbose=*/ false);

                    // hex_dump(gba_edid->data, gba_edid->len);

#ifdef UNNEEDED
                    rpt_vstring(2, "Raw EDID:");
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
                           rpt_vstring(2, "Unable to parse EDID");
                           // printf(" Unparsable EDID for output name: %s -> %p\n", prec->output_name, prec->edidbytes);
                           // hex_dump(prec->edidbytes, 128);
                       }
                    }
#endif

                    // look for i2c-n subdirectory, may or may not be present depending on driver
                    // DBGMSG("cur_dir_name: %s", cur_dir_name);
                    DIR* d2 = opendir(cur_dir_name);
                    char * i2c_node_name = NULL;

                    if (!d2) {
                       rpt_vstring(1, "Unexpected error. Unable to open sysfs directory %s: %s\n",
                                      cur_dir_name, strerror(errno));
                       break;
                    }
                    else {
                       while ((dent2 = readdir(d2)) != NULL) {
                          // DBGMSG("%s", dent2->d_name);
                          if (str_starts_with(dent2->d_name, "i2c")) {
                             rpt_vstring(2, "I2C device: %s", dent2->d_name);
                             i2c_node_name = strdup(dent2->d_name);
                             break;
                          }
                       }
                       closedir(d2);
                    }

                    // rpt_nl();

                    Device_Id_Xref * xref = device_xref_get(gba_edid->data);
                    // xref->sysfs_drm_name = strdup(dent->d_name);
                    xref->sysfs_drm_name = strdup(cur_dir_name);
                    xref->sysfs_drm_i2c  = i2c_node_name;

                    g_byte_array_free(gba_edid, true);

                 }
                 rpt_nl();
               }
            }
            closedir(d);
         }

      }
      if (cardno==0)
         rpt_vstring(1, "No drm class cards found in %s", dname);
      // closedir(d);

   }

   rpt_title("Query file system for i2c nodes under /sys/class/drm/card*...", 1);
   execute_shell_cmd_rpt("ls -ld /sys/class/drm/card*/card*/i2c*", 1);
}

