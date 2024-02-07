/** @file drm_common.c
 *
 *  Consolidates DRM functions variants the have proliferated in the code base.
 */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <glib-2.0/glib.h>
#include <fcntl.h>    // for all_displays_drm2()
#include <inttypes.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // for close() used by probe_dri_device_using_drm_api
#include <xf86drm.h>
#include <xf86drmMode.h>
/** \endcond */

#include "coredefs_base.h"
#include "data_structures.h"
#include "debug_util.h"
#include "file_util.h"
#include "subprocess_util.h"
#include "report_util.h"
#include "string_util.h"
#include "sysfs_filter_functions.h"
#include "sysfs_util.h"

#include "drm_common.h"

// from i2c_sysfs.c

 /** Checks if DRM is supported for a busid.
  *
  * Takes a bus id of the form: PCI:xxxx:xx:xx:d, <drm bus type name>:domain:bus:dev.func
  *
  * @param busid2  DRM PCI bus id
  */
 bool check_drm_supported_using_drm_api(char * busid2) {
    bool debug = false;
    bool supports_drm = false;
       // Notes from examining the code for drmCheckModesettingAvailable()
       //
       // Checks if a modesetting capable driver has been attached to the pci id
       // n.b. drmCheckModesettingSupport() takes a busid string as argument, not filename
       //
       // Returns 0       if bus id valid and modesetting supported
       //         -EINVAL if invalid bus id
       //         -ENOSYS if no modesetting support
       // does not set errno

       int rc = drmCheckModesettingSupported(busid2);
       DBGF(debug,
              "drmCheckModesettingSupported() returned %d for %s", rc, busid2);
       switch (rc) {
       case (0):
              supports_drm = true;
              break;
       case (-EINVAL):
              DBGF(debug,  "Invalid bus id (-EINVAL)");
              break;
       case (-ENOSYS):
              DBGF(debug,  "Modesetting not supported (-ENOSYS)");
              break;
       default:
           DBGF(debug,
                 "drmCheckModesettingSupported() returned undocumented status code %d", rc);
       }
       return supports_drm;
  }


// from i2c/i2c_sysfs.c
 /** Checks if a video adapter supports DRM, using DRM functions.
  *
  *  @param   adapter_path  fully qualified path of video adapter node in sysfs
  *  @retval  true   driver supports DRM
  *  @@retval false  driver does not support DRM, or ddcutil not built with DRM support
  */
 bool adapter_supports_drm_using_drm_api(const char * adapter_path) {
    bool debug = false;
    DBGF(debug, "Starting. adapter_path=%s", adapter_path);
    bool result = false;
 #ifdef USE_LIBDRM
       char * adapter_basename = g_path_get_basename(adapter_path);
       char buf[20];
       g_snprintf(buf, 20, "pci:%s", adapter_basename);
       free(adapter_basename);
       result = check_drm_supported_using_drm_api(buf);
 #endif
    DBGF(debug, "Done.    Returning: %s", sbool(result));
    return result;
 }


 /** Checks if all video adapters in an array of sysfs adapter paths
  *  support DRM
  *
  *  @oaram  adapter_paths  array of paths to adapter nodes in sysfs
  *  @return true if all adapters support DRM, false if not or the array is empty
  */
 bool all_video_adapters_support_drm_using_drm_api(GPtrArray * adapter_paths) {
    bool debug = false;
    DBGF(debug, "Starting. adapter_paths->len=%d", adapter_paths->len);
    bool result = false;
    if (adapter_paths && adapter_paths->len > 0) {
       result = true;
       for (int ndx = 0; ndx < adapter_paths->len; ndx++) {
          result &= adapter_supports_drm_using_drm_api(g_ptr_array_index(adapter_paths, ndx));
       }
    }
    DBGF(debug, "Done.  Returning: %s", sbool(result));
    return result;
 }


 // from util/libdrm_util.c

 static char * drm_bus_type_name(uint8_t bus) {
    char * result = NULL;
    if (bus == DRM_BUS_PCI)
       result = "pci";
    else
       result = "unk";
    return result;
 }


 /* Filter to find driN files using scandir() in get_filenames_by_filter() */
 static int is_dri2(const struct dirent *ent) {
    return str_starts_with(ent->d_name, "card");
 }


 /* Scans /dev/dri to obtain list of device names
  *
  * Returns:   GPtrArray of device names.
  */
 GPtrArray * get_dri_device_names_using_filesys() {
    const char *dri_paths[] = { "/dev/dri/", NULL };
    GPtrArray* dev_names = get_filenames_by_filter(dri_paths, is_dri2);
    g_ptr_array_sort(dev_names, gaux_ptr_scomp);   // needed?
    return dev_names;
 }


 bool probe_dri_device_using_drm_api(const char * devname) {
    bool debug = false;
    DBGF(debug, "Starting. devname = %s", devname);

    bool supports_drm = false;
    // int fd  = open(devname,O_RDWR | O_CLOEXEC);   // WTF? O_CLOEXEC undeclared, works in query_sysenv.c
    int fd  = open(devname,O_RDWR);
    if (fd < 0) {
       DBGF(debug,  "Error opening device %s using open(), errno=%d", devname, errno);
    }
    else {
       DBGF(debug,  "Open succeeded for device: %s", devname);
       char busid2[30] = "";

       struct _drmDevice * ddev;
       // gets information about the opened DRM device
       // returns 0 on success, negative error code otherwise
       int get_device_rc = drmGetDevice(fd, &ddev);
       if (get_device_rc < 0) {
          DBGF(debug,  "drmGetDevice() returned %d", get_device_rc);
       }
       else {
          snprintf(busid2, sizeof(busid2), "%s:%04x:%02x:%02x.%d",
                 drm_bus_type_name(ddev->bustype),
                 ddev->businfo.pci->domain,
                 ddev->businfo.pci->bus,
                 ddev->businfo.pci->dev,
                 ddev->businfo.pci->func);
          DBGF(debug,  "domain:bus:device.func: %04x:%02x:%02x.%d",
                 ddev->businfo.pci->domain,
                 ddev->businfo.pci->bus,
                 ddev->businfo.pci->dev,
                 ddev->businfo.pci->func);

          supports_drm = check_drm_supported_using_drm_api(busid2);

          drmFreeDevice(&ddev);
       }
       close(fd);  // because O_CLOEXEC not recognized
    }
    DBGF(debug, "Done. Returning: %s", sbool(supports_drm));
    return supports_drm;
 }


/** Checks if all display adapters support DRM.
 *
 *  For each file in /dev/dri, use DRM API to ensure that
 *  DRM is supported by using the drm api.
 *
 *  @return true if all adapters support DRM
 */
 bool all_displays_drm_using_drm_api() {
    bool debug = false;
    DBGF(debug,  "Starting");

    bool result = false;
    int drm_available = drmAvailable();
    // DBGF(debug, "drmAvailable() returned:  %d", drm_available);
    if (drm_available) {
       GPtrArray * dev_names = get_dri_device_names_using_filesys();
       if (dev_names->len > 0)
          result = true;
       for (int ndx = 0; ndx < dev_names->len; ndx++) {
          char * dev_name = g_ptr_array_index(dev_names, ndx);
          if (! probe_dri_device_using_drm_api( dev_name))
             result = false;
       }
       g_ptr_array_free(dev_names, true);
    }
    DBGF(debug,  "Done. Returning: %s", sbool(result));
    return result;
 }


// from sysfs_i2c_util.c

#ifdef UNUSED
static void
do_sysfs_drm_card_number_dir(
            const char * dirname,     // <device>/drm
            const char * simple_fn,   // card0, card1, etc.
            void *       data,
            int          depth)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. dirname=%s, simple_fn=%s\n", __func__, dirname, simple_fn);
   Bit_Set_256 * card_numbers = (Bit_Set_256*) data;
   const char * s = simple_fn+4;
   int card_number = atoi(s);
   *card_numbers = bs256_insert(*card_numbers, card_number);
   if (debug)
      printf("(%s) Done.    Added %d\n", __func__, card_number);
}


Bit_Set_32
get_sysfs_drm_card_numbers() {
   bool debug = false;
   char * dname =
 #ifdef TARGET_BSD
              "/compat/linux/sys/class/drm";
 #else
              "/sys/class/drm";
 #endif
   if (debug)
      printf("(%s) Examining %s\n", __func__, dname);
   Bit_Set_32 result = EMPTY_BIT_SET_32;
   dir_foreach(
                 dname,
                 predicate_cardN,            // filter function
                 do_sysfs_drm_card_number_dir,
                 &result,                 // accumulator
                 0);
   if (debug)
      printf("(%s) Done.    Returning DRM card numbers: %s\n", __func__, bs32_to_string_decimal(result, "", ", "));
   return result;
 }
#endif


// Beginning of get_video_devices2() segment
// Use C code instead of bash command to find all subdirectories
// of /sys/devices having class x03

#ifdef UNUSED
bool not_ata(const char * simple_fn) {
   return !str_starts_with(simple_fn, "ata");
}
#endif

bool is_pci_dir(const char * simple_fn) {
   bool debug = false;
   bool result = str_starts_with(simple_fn, "pci0");
   DBGF(debug, "simple_fn = %s, returning %s", simple_fn, sbool(result));
   return result;
}


bool predicate_starts_with_0(const char * simple_fn) {
   bool debug = false;
   bool result = str_starts_with(simple_fn, "0");
   DBGF(debug, "simple_fn = %s, returning %s", simple_fn, sbool(result));
   return result;
}


void find_class_dirs(const char * dirname,
                     const char * simple_fn,
                     void *       accumulator,
                     int          depth)
{
    bool debug = false;
    DBGF(debug, "Starting. dirname=%s, simple_fn=%s, accumulator=%p, depth=%d",
          dirname, simple_fn, accumulator, depth);
    char * subdir = g_strdup_printf("%s/%s", dirname, simple_fn);
    GPtrArray* accum = accumulator;
    char * result = NULL;
    bool found = RPT_ATTR_TEXT(-1, &result, dirname, simple_fn, "class");
    if (found) {
       DBGF(debug, "subdir=%s has attribute class = %s. Adding.", subdir, result);
       g_ptr_array_add(accum, (char*) subdir);
    }
    else {
       DBGF(debug, "subdir=%s does not have attribute class", subdir);
    }
    DBGF(debug, "Examining subdirs of %s", subdir);
    dir_foreach(subdir, predicate_starts_with_0, find_class_dirs, accumulator, depth+1);
}


/** Returns the paths to all video devices in /sys/devices, i.e. those
 *  subdirectories (direct or indirect) having class = 0x03
 *
 *  @return array of directory names, caller must free
 */
GPtrArray *  get_video_adapter_devices2() {
   bool debug = false;
   DBGF(debug, "Starting.");
   GPtrArray * class03_dirs = g_ptr_array_new_with_free_func(g_free);
   dir_foreach("/sys/devices", is_pci_dir, find_class_dirs, class03_dirs, 0);
   if (debug) {
      DBG("Before filtering: class03_dirs->len =%d", class03_dirs->len);
      for (int ndx = 0; ndx < class03_dirs->len; ndx++) {
         rpt_vstring(2, "%s", g_ptr_array_index(class03_dirs, ndx));
      }
   }
   for (int ndx = class03_dirs->len -1; ndx>= 0; ndx--) {
      char * dirname =  g_ptr_array_index(class03_dirs, ndx);
      DBGF(debug, "dirname=%s", dirname);
      char * class = NULL;
      int d = (debug) ? 1 : -1;
      RPT_ATTR_TEXT(d, &class, dirname, "class");
      assert(class);
      if ( !str_starts_with(class, "0x03") ) {
         g_ptr_array_remove_index(class03_dirs, ndx);
      }
   }

   if (debug) {
      DBG("Returning %d directories:", class03_dirs->len);
      for (int ndx = 0; ndx < class03_dirs->len; ndx++)
         rpt_vstring(2, "%s", (char*) g_ptr_array_index(class03_dirs, ndx));
   }

   return class03_dirs;
}


/** Returns the paths to all video devices in /sys/devices, i.e. those
 *  subdirectories (direct or indirect) having class = 0x03
 *
 *  @return array of directory names, caller must free
 */
GPtrArray * get_video_adapter_devices() {
   bool debug = false;
   char * cmd = "find /sys/devices -name class | xargs grep x03 -l | sed 's|class||'";
   GPtrArray * result = execute_shell_cmd_collect(cmd);
   g_ptr_array_set_free_func(result, g_free);

   if (debug) {
      DBG("Returning %d directories:", result->len);
      for (int ndx = 0; ndx < result->len; ndx++)
         rpt_vstring(2, "%s", (char*) g_ptr_array_index(result, ndx));
   }

   if (debug) {
      // For testing:
      GPtrArray* devices2 = get_video_adapter_devices2();
      DBG("get_video_adapter_devices2 returned %d directories:", devices2->len);
      for (int ndx = 0; ndx < devices2->len; ndx++)
         rpt_vstring(2, "%s", (char*) g_ptr_array_index(devices2, ndx));
      g_ptr_array_free(devices2, true);
   }

   return result;
}


typedef struct {
   bool has_card_connector_dir;
} Check_Card_Struct;

#ifdef REF
typedef void (*Dir_Foreach_Func)(
const char *  dirname,
const char *  fn,
void *        accumulator,
int           depth);
#endif


void dir_foreach_set_true(const char * dirname, const char * fn, void * accumulator, int depth) {
   bool debug = false;
   DBGF(debug, "dirname=%s, fn=%s, accumlator=%p, depth=%d", dirname, fn, accumulator, depth);
   Check_Card_Struct * accum = accumulator;
   DBGF(debug, "Setting accumulator->has_card_connector_dir = true");
   accum->has_card_connector_dir = true;
}


void do_one_card(const char * dirname, const char * fn, void* accumulator, int depth) {
   bool debug = false;
   char buf[PATH_MAX];
   g_snprintf(buf, sizeof(buf), "%s/%s", dirname, fn);
   DBGF(debug, "Examining dir buf=%s", buf);
   dir_foreach(buf, predicate_cardN_connector, dir_foreach_set_true, accumulator, depth);
   Check_Card_Struct * accum = accumulator;
   DBGF(debug, "Finishing with accumlator->has_card_connector_dir = %s", sbool(accum->has_card_connector_dir));
}


// n. could enhance to collect the card connector subdir names
bool card_connector_subdirs_exist(const char * adapter_dir) {
   bool debug = false;
   DBGF(debug, "Starting. adapter_dir = %s", adapter_dir);
   int lastpos= strlen(adapter_dir) - 1;
   char * delim = (adapter_dir[lastpos] == '/') ? "" : "/";
   char drm_dir[PATH_MAX];
   g_snprintf(drm_dir, PATH_MAX, "%s%sdrm", adapter_dir,delim);
   DBGF(debug, "drm_dir=%s", drm_dir);
   int d = (debug) ? 1 : -1;
   Check_Card_Struct *  accumulator = calloc(1, sizeof(Check_Card_Struct));
   dir_foreach(drm_dir, predicate_cardN, do_one_card, accumulator, d);
   bool has_card_subdir = accumulator->has_card_connector_dir;
   free(accumulator);
   DBGF(debug, "Done.    Returning %s", sbool(has_card_subdir));
   return has_card_subdir;
}


/** Check that all devices in a list of video adapter devices have drivers that implement
 *  drm by looking for card_connector_dirs in each adapter's drm directory.
 *
 *  @param  adapter_devices array of /sys directory names for video adapter devices
 *  @return true if all adapter video drivers implement drm
 */
bool check_video_adapters_list_implements_drm(GPtrArray * adapter_devices) {
   bool debug = false;
   assert(adapter_devices);
   // DBGF(debug, "adapter_devices->len=%d at %p", adapter_devices->len, adapter_devices);
   bool result = true;
   for (int ndx = 0; ndx < adapter_devices->len; ndx++) {
      // char * subdir_name = NULL;
      char * adapter_dir = g_ptr_array_index(adapter_devices, ndx);
      bool has_card_subdir = card_connector_subdirs_exist(adapter_dir);
      // DBGF(debug, "Examined.  has_card_subdir = %s", sbool(has_card_subdir));
      if (!has_card_subdir) {
         result = false;
         break;
      }
   }
   DBGF(debug, "Done.     Returning %s", sbool(result));
   return result;
}


/** Checks that all video adapters on the system have drivers that implement drm
 *  by checking that card connector directories drm/cardN/cardN-xxx exist.
 *
 *  @return true if all video adapters have drivers implementing drm, false if not
 */
bool check_all_video_adapters_implement_drm() {
   bool debug = false;
   DBGF(debug, "Starting");

   GPtrArray * devices = NULL;
   devices = get_video_adapter_devices();

   // g_ptr_array_free(devices, true);
   //   devices = get_video_adapter_devices2();   // FAILS

    // DBGF(debug, "%d devices at %p:", devices->len, devices);
    // for (int ndx = 0; ndx < devices->len; ndx++)
    //    rpt_vstring(2, "%s", g_ptr_array_index(devices, ndx));

   bool all_drm = check_video_adapters_list_implements_drm(devices);
   g_ptr_array_free(devices, true);

   DBGF(debug, "Done.  Returning %s", sbool(all_drm));
   return all_drm;
}


#ifdef TO_FIX
/** Checks if a display has a DRM driver by looking for
 *  card connector subdirs of drm in the adapter directory.
 *
 *  @param busno   I2C bus number
 *  @return true/false
 */
 bool is_drm_display_by_busno(int busno) {
   bool debug = false;
   DBGF(debug, "Starting. busno = %d", busno);
   bool result = false;
   char i2cdir[40];
   g_snprintf(i2cdir, 40, "/sys/bus/i2c/devices/i2c-%d",busno);
   char * real_i2cdir = NULL;
   GET_ATTR_REALPATH(&real_i2cdir, i2cdir);
   DBGF(debug, "real_i2cdir = %s", real_i2cdir);
   assert(real_i2cdir);
   int d = (debug)  ? 1 : -1;
   char * adapter_dir = find_adapter(real_i2cdir, d);  // in i2c/i2c_sysfs.c, need to fix
   assert(adapter_dir);
   result = card_connector_subdirs_exist(adapter_dir);
   free(real_i2cdir);
   free(adapter_dir);
   DBGF(debug, "Done.    Returning: %s", sbool(result));
   return result;
}
#endif



