/** @file libdrm_aux_util.c
 *
 *  Functions that depend on the DRM API.
 */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include "timestamp.h"

#include "libdrm_aux_util.h"

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
       char * adapter_basename = g_path_get_basename(adapter_path);
       char buf[20];
       g_snprintf(buf, 20, "pci:%s", adapter_basename);
       free(adapter_basename);
       result = check_drm_supported_using_drm_api(buf);
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
  * Returns:   GPtrArray of device names, caller must free
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
 *
 *  @remark: unreliable on Wayland!?
 */
 bool all_displays_drm_using_drm_api() {
    bool debug = false;
    DBGF(debug,  "Starting");

    bool result = false;
    // returns false on banner under Wayland!!!!
    int drm_available = drmAvailable();
    DBGF(debug, "drmAvailable() returned:  %d", drm_available);
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


