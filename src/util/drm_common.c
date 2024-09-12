/** @file drm_common.c
 *
 *  Consolidates DRM function variants the have proliferated in the code base.
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
#include <libdrm/drm_mode.h>
/** \endcond */

#include "coredefs_base.h"
#include "data_structures.h"
#include "debug_util.h"
#include "file_util.h"
#include "subprocess_util.h"
#include "regex_util.h"
#include "report_util.h"
#include "string_util.h"
#include "sysfs_filter_functions.h"
#include "sysfs_i2c_util.h"
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
    assert(adapter_path);
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


const char * drm_bus_type_name(uint8_t bus) {
    char * result = NULL;

    switch(bus) {
           case DRM_BUS_PCI:      result = "pci";      break; // 0
           case DRM_BUS_USB:      result = "usb";      break; // 1
           case DRM_BUS_PLATFORM: result = "platform"; break; // 2
           case DRM_BUS_HOST1X:   result = "host1x";   break; // 3
           default:               result = "unrecognized";
    }

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
          DBGF(debug,  "drmGetDevice() returned %d = %s", get_device_rc, strerror(-get_device_rc));
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

          DBGF(debug, "busid2 = |%s|", busid2);
          supports_drm = check_drm_supported_using_drm_api(busid2);

          drmFreeDevice(&ddev);
       }
       close(fd);
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
 *
 *  The degenerate case of no video adapters returns false.
 *
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




 Value_Name_Title drm_connector_type_table[] = {
    VNT(DRM_MODE_CONNECTOR_Unknown     , "unknown"    ), //  0
    VNT(DRM_MODE_CONNECTOR_VGA         , "VGA"        ), //  1
    VNT(DRM_MODE_CONNECTOR_DVII        , "DVI-I"      ), //  2
    VNT(DRM_MODE_CONNECTOR_DVID        , "DVI-D"      ), //  3
    VNT(DRM_MODE_CONNECTOR_DVIA        , "DVI-A"      ), //  4
    VNT(DRM_MODE_CONNECTOR_Composite   , "Composite"  ), //  5
    VNT(DRM_MODE_CONNECTOR_SVIDEO      , "S-video"    ), //  6
    VNT(DRM_MODE_CONNECTOR_LVDS        , "LVDS"       ), //  7
    VNT(DRM_MODE_CONNECTOR_Component   , "Component"  ), //  8
    VNT(DRM_MODE_CONNECTOR_9PinDIN     , "DIN"        ), //  9
    VNT(DRM_MODE_CONNECTOR_DisplayPort , "DP"         ), // 10
    VNT(DRM_MODE_CONNECTOR_HDMIA       , "HDMI"       ), // 11
    VNT(DRM_MODE_CONNECTOR_HDMIB       , "HDMI-B"     ), // 12
    VNT(DRM_MODE_CONNECTOR_TV          , "TV"         ), // 13
    VNT(DRM_MODE_CONNECTOR_eDP         , "eDP"        ), // 14
    VNT(DRM_MODE_CONNECTOR_VIRTUAL     , "Virtual"    ), // 15
    VNT(DRM_MODE_CONNECTOR_DSI         , "DSI"        ), // 16  Display Signal Interface, used on Raspberry Pi
    VNT_END
 };


 /** Returns the symbolic name of a connector type.
  * @param val connector type
  * @return symbolic name
  */
 char * drm_connector_type_name(Byte val) {
    return vnt_name(drm_connector_type_table, val);
 }


 /** Returns the description string for a connector type.
  * @param val connector type
  * @return descriptive string
  */
 char * drm_connector_type_title(Byte val) {
    return vnt_title(drm_connector_type_table, val);
 }




// For getting the DRM connector type from the DRM connector name

   Value_Name_Title connector_type_lookup_table[] = {
       VNT(DRM_MODE_CONNECTOR_Unknown     , "unknown"    ), //  0
       VNT(DRM_MODE_CONNECTOR_VGA         , "VGA"        ), //  1
       VNT(DRM_MODE_CONNECTOR_DVII        , "DVII"      ), //  2
       VNT(DRM_MODE_CONNECTOR_DVID        , "DVID"      ), //  3
       VNT(DRM_MODE_CONNECTOR_DVIA        , "DVIA"      ), //  4
       VNT(DRM_MODE_CONNECTOR_Composite   , "Composite"  ), //  5
       VNT(DRM_MODE_CONNECTOR_SVIDEO      , "Svideo"    ), //  6
       VNT(DRM_MODE_CONNECTOR_LVDS        , "LVDS"       ), //  7
       VNT(DRM_MODE_CONNECTOR_Component   , "Component"  ), //  8
       VNT(DRM_MODE_CONNECTOR_9PinDIN     , "DIN"        ), //  9
       VNT(DRM_MODE_CONNECTOR_DisplayPort , "DP"         ), // 10
       VNT(DRM_MODE_CONNECTOR_HDMIA       , "HDMI"       ), // 11  alternate common name for HDMIA
       VNT(DRM_MODE_CONNECTOR_HDMIA       , "HDMIA"       ), // 11
       VNT(DRM_MODE_CONNECTOR_HDMIB       , "HDMIB"     ), // 12
       VNT(DRM_MODE_CONNECTOR_TV          , "TV"         ), // 13
       VNT(DRM_MODE_CONNECTOR_eDP         , "eDP"        ), // 14
       VNT(DRM_MODE_CONNECTOR_VIRTUAL     , "Virtual"    ), // 15
       VNT(DRM_MODE_CONNECTOR_DSI         , "DSI"        ), // 16  Display Signal Interface, used on Raspberry Pi
       VNT(DRM_MODE_CONNECTOR_DPI         , "DPI"        ), // 17
       VNT(DRM_MODE_CONNECTOR_WRITEBACK   , "WRITEBACK"  ), // 18
       VNT(DRM_MODE_CONNECTOR_SPI         , "SPI"        ), // 19
       VNT(DRM_MODE_CONNECTOR_USB         , "USB"        ), // 20
       VNT_END
    };


int lookup_connector_type(const char * name) {
   int val = vnt_find_id(
         connector_type_lookup_table,
         name,
         true,     // search by title
         true,     // ignore_case,
         -1);      // default_id
   return val;
}


char * dci_repr(Drm_Connector_Identifier dci) {
   char * buf = g_strdup_printf("[dci:cardno=%d,connector_id=%d,connector_type=%d=%s,connector_type_id=%d]",
         dci.cardno, dci.connector_id,
         dci.connector_type, drm_connector_type_name(dci.connector_type),
         dci.connector_type_id);
   return buf;
}


/** Thread safe function that returns a brief string representation of a #Drm_Connector_Identifier.
 *  The returned value is valid until the next call to this function on the current thread.
 *
 *
 *  \param  dpath  pointer to ##DDCA_IO_Path
 *  \return string representation of #DDCA_IO_Path
 */
char * dci_repr_t(Drm_Connector_Identifier dci) {
   static GPrivate  dci_repr_key = G_PRIVATE_INIT(g_free);

   char * repr = dci_repr(dci);
   char * buf = get_thread_fixed_buffer(&dci_repr_key, 100);
   g_snprintf(buf, 100, "%s", repr);
   free(repr);

   return buf;
}


bool dci_eq(Drm_Connector_Identifier dci1, Drm_Connector_Identifier dci2) {
   bool result = false;
   if (dci1.connector_id > 0 && dci1.connector_id == dci2.connector_id) {
      result = true;
   }
   else
      result = dci1.cardno            == dci2.cardno &&
               dci1.connector_type    == dci2.connector_type &&
               dci1.connector_type_id == dci2.connector_type_id;
   return result;
}


int dci_cmp(Drm_Connector_Identifier dci1, Drm_Connector_Identifier dci2) {
   int result = 0;
   if (dci1.cardno < dci2.cardno)
      result = -1;
   else if (dci1.cardno > dci2.cardno)
      result = 1;
   else {
      if (dci1.connector_type < dci2.connector_type)
         result = -1;
      else if (dci1.connector_type > dci2.connector_type)
         result = 1;
      else {
         if (dci1.connector_type_id < dci2.connector_type_id)
            result = -1;
         else if (dci1.connector_type_id > dci2.connector_type_id)
            result = 1;
         else
            result = 0;
      }
   }
   return result;
}

int sys_drm_connector_name_cmp0(const char * s1, const char * s2) {
   int result = 0;

   // do something "reasonable" for pathological cases
   if (!s1 && s2)
      result = -1;
   else if (!s1 && !s2)
      result = 0;
   else if (s1 && !s2)
      result = 1;

   else {      // normal case
      Drm_Connector_Identifier dci1 = parse_sys_drm_connector_name(s1);
      Drm_Connector_Identifier dci2 = parse_sys_drm_connector_name(s2);
      result = dci_cmp(dci1, dci2);
   }

   return result;
}



int sys_drm_connector_name_cmp(gconstpointer connector_name1, gconstpointer connector_name2) {
   bool debug = false;

   int result = 0;
   char * s1 = (connector_name1) ? *(char**)connector_name1 : NULL;
   char * s2 = (connector_name2) ? *(char**)connector_name2 : NULL;
   DBGF(debug, "s1=%p->%s, s2=%p->%s", s1, s1, s2, s2);

   result = sys_drm_connector_name_cmp0(s1, s2);

   DBGF(debug, "Returning: %d", result);
   return result;
}


Drm_Connector_Identifier parse_sys_drm_connector_name(const char * drm_connector) {
   bool debug = false;
   DBGF(debug, "Starting. drm_connector = |%s|", drm_connector);
   Drm_Connector_Identifier result = {-1,-1,-1,-1};
   static const char * drm_connector_pattern = "^card([0-9])[-](.*)[-]([0-9]+)";

   regmatch_t  matches[4];

   bool ok =  compile_and_eval_regex_with_matches(
         drm_connector_pattern,
         drm_connector,
         4,   //       max_matches,
         matches);

   if (ok) {
      // for (int kk = 0; kk < 4; kk++) {
      //    rpt_vstring(1, "match %d, substring start=%d, end=%d", kk, matches[kk].rm_so, matches[kk].rm_eo);
      // }
      char * cardno_s = substr(drm_connector, matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
      char * connector_type = substr(drm_connector, matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
      char * connector_type_id_s = substr(drm_connector, matches[3].rm_so, matches[3].rm_eo - matches[3].rm_so);
      // DBGF(debug, "cardno_s=|%s|", cardno_s);
      // DBGF(debug, "connector_type=|%s|", connector_type);
      // DBGF(debug, "connector_type_id_s=|%s|", connector_type_id_s);

      ok = str_to_int(cardno_s, &result.cardno, 10);
      assert(ok);
      ok = str_to_int(connector_type_id_s, &result.connector_type_id, 10);
      assert(ok);
      // DBGF(debug, "result.cardno: %d", result.cardno);
      // DBGF(debug, "connector_type_id: %d", result.connector_type_id);

      result.connector_type = lookup_connector_type(connector_type);

      free(connector_type);
      free(cardno_s);
      free(connector_type_id_s);
   }

   if (debug) {
      char * s = dci_repr(result);
      DBG("Done.     Returning: %s", s);
      free(s);
   }
   return result;
}



