/** @file query_drm_sysenv.c
 *
 *  drm reporting for the environment command
 */

// Copyright (C) 2017-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


// #define _GNU_SOURCE

/** \cond */
#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

// NO.  We want the versions in /usr/include/libdrm
#ifdef NO
#include <drm/drm.h>
#include <drm/drm_mode.h>
#endif

#include "util/edid.h"
#include "util/file_util.h"
#include "util/glib_util.h"
#include "util/libdrm_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/subprocess_util.h"

#include "base/core.h"
#include "base/linux_errno.h"
/** \endcond */

#include "query_sysenv_base.h"
#include "query_sysenv_xref.h"

#include "query_sysenv_drm.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_ENV;

#ifdef REF  // local copy for reference
#define DRM_BUS_PCI   0

typedef struct _drmPciBusInfo {
    uint16_t domain;
    uint8_t bus;
    uint8_t dev;
    uint8_t func;
} drmPciBusInfo, *drmPciBusInfoPtr;

typedef struct _drmPciDeviceInfo {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subvendor_id;
    uint16_t subdevice_id;
    uint8_t revision_id;
} drmPciDeviceInfo, *drmPciDeviceInfoPtr;

typedef struct _drmDevice {
    char **nodes; /* DRM_NODE_MAX sized array */
    int available_nodes; /* DRM_NODE_* bitmask */
    int bustype;
    union {
        drmPciBusInfoPtr pci;
    } businfo;
    union {
        drmPciDeviceInfoPtr pci;
    } deviceinfo;
} drmDevice, *drmDevicePtr;

extern int drmGetDevice(int fd, drmDevicePtr *device);
extern void drmFreeDevice(drmDevicePtr *device);
#endif


#ifdef UNUSED
// can't find basename, roll our own
char * basename0(char * fn) {
   char * result = NULL;
   if (fn) {
      int l = strlen(fn);
      if (l == 0)
         result = fn;
      else {
         char * p = fn+(l-1);
         while (*p != '/') {
            if (p == fn) {
               break;
            }
            p--;
         }
         result = (p == fn) ? p : p+1;;
      }
   }
   return result;
}
#endif


static char * drm_bus_type_name(uint8_t bus) {
   char * result = NULL;
   if (bus == DRM_BUS_PCI)
      result = "pci";
   else
      result = "unk";
   return result;
}


static void report_drmVersion(drmVersion * vp, int depth) {
   rpt_vstring(depth, "Version:     %d.%d.%d",
                      vp->version_major, vp->version_minor, vp->version_patchlevel);
   rpt_vstring(depth, "Driver:      %.*s", vp->name_len, vp->name);
   rpt_vstring(depth, "Date:        %.*s", vp->date_len, vp->date);
   rpt_vstring(depth, "Description: %.*s", vp->desc_len, vp->desc);
}


/* Examines a single open DRM device.
 *
 * Arguments:
 *   fd      file handle of open DRM device
 *   depth   logical indentation depth
 *
 * Returns:  nothing
 */
static void probe_open_device_using_libdrm(int fd, int depth) {
   int d1 = depth+1;
   int d2 = depth+2;
   int d3 = depth+3;
   bool debug = false;
   int rc;
   char * busid = NULL;
   // int errsv;

   rpt_nl();

   DBGTRC(debug, TRACE_GROUP, "Starting. fd=%d", fd);

   // succeeds if run as root, fails w errno=EACCES(13) if not
   // but no effect on subsequent failures for nvidia
   // reviewed code in drm_ioctl.c.  ioctl calls would fail with EACCES
   // if lack of master access were the cause
#ifdef NO
   // fails on suse
   // rc is ioctl() return code, i.e. 0 or -1 for failure)
   // if -1, errno is set
   rc = drmSetMaster(fd);
   if (rc < 0)
      rpt_vstring(d1, "drmSetMaster() failed, errno=%s", linux_errno_desc(errno));
   rpt_nl();
#endif

   // if returns NULL, errno is as set from the underlying ioctl()
   drmVersionPtr vp = drmGetVersion(fd);
   if (vp) {
      rpt_vstring(d1, "DRM driver version information:");
      report_drmVersion(vp, d2);
      drmFreeVersion(vp);
   }
   else {
      rpt_vstring(d1, "Error calling drmGetVersion().  errno=%s", linux_errno_desc(errno));
   }
   rpt_nl();

   // fills in a hardcoded version number (currently 1.3.0), never fails
   // only fills in the major, minor, and patchLevel fields, others are always 0
   vp = drmGetLibVersion(fd);
   // rpt_vstring(d1, "DRM library version information:");
   // report_drmVersion(vp, d2);
   rpt_vstring(d1, "DRM library version: %d.%d.%d.",
                    vp->version_major, vp->version_minor, vp->version_patchlevel);
   drmFreeVersion(vp);
   rpt_nl();


   // returns null string if open() instead of drmOpen(,busid) used to to open
   // uses successive DRM_IOCTL_GET_UNIQUE calls
   busid = drmGetBusid(fd);
   if (busid) {
      rpt_vstring(d1, "DRM Busid:  %s", busid);
      // drmFreeBusid(busid);  // requires root
      free(busid);
   }
   else {
      rpt_vstring(d1, "Error calling drmGetBusid().  errno=%s", linux_errno_desc(errno));
   }

   char busid2[30] = "";

   rpt_nl();
   struct _drmDevice * ddev;
   // gets information about the opened DRM device
   // returns 0 on success, negative error code otherwise
   rc = drmGetDevice(fd, &ddev);
   if (rc < 0) {
      rpt_vstring(depth, "drmGetDevice() returned %d, interpreted as error code: %s",
                         rc, linux_errno_desc(-rc));
   }
   else {
      rpt_vstring(d1, "Device information:");
      rpt_vstring(d2, "bustype:                %d - %s",
            ddev->bustype, drm_bus_type_name(ddev->bustype));
#ifdef OLD
      rpt_vstring(d2, "bus,dev,domain,func: %d, %d, %d, %d",
            ddev->businfo.pci->bus,
            ddev->businfo.pci->dev,
            ddev->businfo.pci->domain,
            ddev->businfo.pci->func);
#endif
      snprintf(busid2, sizeof(busid2), "%s:%04x:%02x:%02x.%d",
            drm_bus_type_name(ddev->bustype),
            ddev->businfo.pci->domain,
            ddev->businfo.pci->bus,
            ddev->businfo.pci->dev,
            ddev->businfo.pci->func);
      rpt_vstring(d2, "domain:bus:device.func: %04x:%02x:%02x.%d",
            ddev->businfo.pci->domain,
            ddev->businfo.pci->bus,
            ddev->businfo.pci->dev,
            ddev->businfo.pci->func);
      rpt_vstring(d2, "vendor    vid:pid:      0x%04x:0x%04x",
            ddev->deviceinfo.pci->vendor_id,
            ddev->deviceinfo.pci->device_id);
      rpt_vstring(d2, "subvendor vid:pid:      0x%04x:0x%04x",
            ddev->deviceinfo.pci->subvendor_id,
            ddev->deviceinfo.pci->subdevice_id);
      rpt_vstring(d2, "revision id:            0x%04x",
            ddev->deviceinfo.pci->revision_id);
      drmFreeDevice(&ddev);
   }

   if (strlen(busid2) > 0) {
      rpt_nl();


   // Notes from examining the code for drmCheckModesettingAvailable()
   //
   // Checks if a modesetting capable driver has been attached to the pci id
   // n.b. drmCheckModesettingSupport() takes a busid string as argument, not filename
   //
   // Returns 0       if bus id valid and modesetting supported
   //         -EINVAL if invalid bus id
   //         -ENOSYS if no modesetting support
   // does not set errno

    // parses busid using sscanf(busid, "pci:%04x:%02x:%02x.%d", &domain, &bus, &dev, &func);

    rpt_vstring(d1, "Is a modesetting capable driver attached to bus id: %s?", busid2);
    rpt_vstring(d1,"(calling drmCheckModesettingAvailable())");
    rc = drmCheckModesettingSupported(busid2);
    switch (rc) {
    case (0):
          rpt_vstring(d2, "Yes");
          break;
    case (-EINVAL):
          rpt_vstring(d2, "Invalid bus id (-EINVAL)");
          break;
    case (-ENOSYS):
          rpt_vstring(d2, "Modesetting not supported (-ENOSYS)");
          break;
    default:
       rpt_vstring(d2, "drmCheckModesettingSupported() returned undocumented status code %d", rc);
    }
   }

#ifdef REF
   extern drmModePropertyPtr drmModeGetProperty(int fd, uint32_t propertyId);
   extern void drmModeFreeProperty(drmModePropertyPtr ptr);
#endif

   rpt_nl();
   rpt_vstring(d1, "Retrieving DRM resources...");
   drmModeResPtr res = drmModeGetResources(fd);
   if (!res) {
      int errsv = errno;
      rpt_vstring(d1, "Failure retrieving DRM resources, errno=%s", linux_errno_desc(errno));
      if (errsv == EINVAL)
         rpt_vstring(d1,"Driver apparently does not provide needed DRM ioctl calls");
      goto bye;
   }
   if (debug)
      report_drmModeRes(res, d2);

   int edid_prop_id         = 0;
   int subconnector_prop_id = 0;
   drmModePropertyPtr edid_prop_ptr   = NULL;
   drmModePropertyPtr subconn_prop_ptr = NULL;

   rpt_nl();
   rpt_vstring(d1, "Scanning defined properties...");
   for (int prop_id = 0; prop_id < 200; prop_id++) {
      drmModePropertyPtr prop_ptr = drmModeGetProperty(fd, prop_id);
      if (prop_ptr) {
         if (debug)
            report_drm_modeProperty(prop_ptr, d2);
         else {
            // TMI
            // summarize_drm_modeProperty(prop_ptr, d2);
         }

         // printf("prop_id=%d\n", prop_id);
         if (streq(prop_ptr->name, "EDID")) {
            // rpt_vstring(d1, "Found EDID property, prop_id=%d, prop_ptr->prop_id=%u", prop_id, prop_ptr->prop_id);
            edid_prop_id = prop_id;
            edid_prop_ptr = prop_ptr;
         }
         else if (streq(prop_ptr->name, "subconnector")) {
            // rpt_vstring(d1, "Found subconnector property, prop_id=%d, prop_ptr->prop_id=%u", prop_id, prop_ptr->prop_id);
            subconnector_prop_id = prop_id;
            subconn_prop_ptr = prop_ptr;
         }
         else {
            drmModeFreeProperty(prop_ptr);
         }
      }
   }

#ifdef REF
   extern drmModePropertyBlobPtr drmModeGetPropertyBlob(int fd, uint32_t blob_id);
   extern void drmModeFreePropertyBlob(drmModePropertyBlobPtr ptr);

   typedef struct _drmModePropertyBlob {
      uint32_t id;
      uint32_t length;
      void *data;
   } drmModePropertyBlobRes, *drmModePropertyBlobPtr;
#endif

   rpt_nl();
   rpt_vstring(d1, "Scanning connectors...");
   for (int i = 0; i < res->count_connectors; ++i) {
      drmModeConnector * conn = drmModeGetConnector(fd, res->connectors[i]);
      if (!conn) {
         rpt_vstring(d1, "Cannot retrieve DRM connector id %d errno=%s",
                         res->connectors[i], linux_errno_desc(errno));
         continue;
      }
      if (debug)
         report_drmModeConnector(fd, conn, d1) ;

      char connector_name[100];
      snprintf(connector_name, 100, "%s-%u",
                                    connector_type_title(conn->connector_type),
                                    conn->connector_type_id);

      rpt_vstring(d1, "%-20s %u",       "connector_id:",      conn->connector_id);
      // rpt_vstring(d2, "%-20s %s-%u",    "connector name",     connector_type_title(conn->connector_type),
      //                                                         conn->connector_type_id);
      rpt_vstring(d2, "%-20s %s",       "connector name",     connector_name);
      rpt_vstring(d2, "%-20s %d - %s",  "connector_type:",    conn->connector_type,
                                                              connector_type_title(conn->connector_type));
      rpt_vstring(d2, "%-20s %d",       "connector_type_id:", conn->connector_type_id);
      rpt_vstring(d2, "%-20s %d - %s",  "connection:",        conn->connection,
                                                              connector_status_title(conn->connection));
      uint32_t encoder_id = conn->encoder_id;     // current encoder
      rpt_vstring(d2, "%-20s %d",       "encoder:",           encoder_id);

      drmModeEncoderPtr penc =  drmModeGetEncoder(fd, encoder_id);
      if (penc) {
         rpt_vstring(d3, "%-20s %d - %s",    "encoder type (signal format):",
                          penc->encoder_type,  encoder_type_title(penc->encoder_type));
      }
      else {
         rpt_vstring(d2, "Encoder with id %d not found", encoder_id);
      }

      for (int ndx = 0; ndx < conn->count_props; ndx++) {
         if (conn->props[ndx] == edid_prop_id) {
            rpt_vstring(d2, "EDID property");
            uint64_t blob_id = conn->prop_values[ndx];
            drmModePropertyBlobPtr blob_ptr = drmModeGetPropertyBlob(fd, blob_id);
            if (!blob_ptr) {
               rpt_vstring(d3, "Blob not found");
            }
            else {
               // printf("blob_ptr->id = %d\n", blob_ptr->id);
               rpt_vstring(d3, "Raw property blob:");
               report_drmModePropertyBlob(blob_ptr, d3);

               if (blob_ptr->length >= 128) {
                  Parsed_Edid * parsed_edid = create_parsed_edid(blob_ptr->data);
                  if (parsed_edid) {
                     report_parsed_edid_base(
                           parsed_edid,
                           true,    // verbose
                           false,   // show_raw
                           d3);
                     free_parsed_edid(parsed_edid);
                  }


                  // DBGMSG("Before xref lookup by edid");
                  // Initial bus scan by I2C device must already have occurred,
                  // to populate the cross-reference table by bus number
                  // bool complete = device_xref_i2c_bus_scan_complete();
                  // DBGMSG("i2c bus scan completed: %s",  sbool(complete) );
                  // assert(complete);
                  // device_xref_report(3);

                  Byte * edidbytes = blob_ptr->data;

#ifdef SYSENV_TEST_IDENTICAL_EDIDS
                  if (first_edid) {
                     DBGMSG("Forcing duplicate EDID");
                     edidbytes = first_edid;
                  }
#endif

                  // Device_Id_Xref * xref = device_xref_get(blob_ptr->data);
                  Device_Id_Xref * xref = device_xref_find_by_edid(edidbytes);
                  if (xref) {
                  // TODO check for multiple entries with same edid
                  xref->drm_connector_name = strdup(connector_name);
                  xref->drm_connector_type = conn->connector_type;
                  // xref->drm_device_path    = strdup(conn->

                  if (xref->ambiguous_edid) {
                     rpt_vstring(d3, "Multiple displays have same EDID ...%s", xref->edid_tag);
                     rpt_vstring(d3, "drm connector name and type for this EDID in device cross reference table may be incorrect");
                  }
                  }
                  else {
                     char * edid_tag = device_xref_edid_tag(edidbytes);
                     DBGMSG("Unexpected: EDID ...%s not found in device cross reference table",
                           edid_tag);
                     free(edid_tag);
                  }
               }

               drmModeFreePropertyBlob(blob_ptr);
            }
         }
         else if (conn->props[ndx] == subconnector_prop_id) {
            assert(subconn_prop_ptr);   // if subconnector_prop_id found, subconn_prop_ptr must have been set
            uint32_t enum_value = conn->prop_values[ndx];
            // printf("subconnector value: %d\n", enum_value);

            // assert(subconn_prop_ptr->flags & DRM_MODE_PROP_ENUM);
            // assert(enum_value < subconn_prop_ptr->count_enums);

            if (subconn_prop_ptr->flags & DRM_MODE_PROP_ENUM) {
               bool found = false;
                for (int i = 0; i < subconn_prop_ptr->count_enums && !found; i++) {
                   if (subconn_prop_ptr->enums[i].value == enum_value) {
                      rpt_vstring(d2, "Subconnector value = %d - %s",
                                      enum_value, subconn_prop_ptr->enums[i].name);
                      found = true;
                   }
                }
                if (!found) {
                   rpt_vstring(d2, "Unrecognized subconnector value: %d", enum_value);
                }
            }
            else {
               rpt_vstring(d2, "Subconnector not type enum!.  Value = %d", enum_value);
            }

         }
#ifdef FUTURE
         else {
            drmModePropertyPtr prop_ptr = drmModeGetProperty(fd, conn->props[ndx]);
            if (prop_ptr) {
               // now in connector report function
               // report_property_value(dri_fd, prop_ptr, conn->prop_values[ndx], 1);
               // to implement:
               summarize_property_value(fd, prop_ptr, conn->prop_values[ndx], d2);
               drmModeFreeProperty(prop_ptr);
            }
            else {
               printf("Unrecognized property id: %d\n", conn->props[ndx]);
            }

         }
#endif

      }
      rpt_nl();
   }

   if (edid_prop_ptr) {
      drmModeFreeProperty(edid_prop_ptr);
   }
   if (subconn_prop_ptr) {
      drmModeFreeProperty(subconn_prop_ptr);
   }

bye:
   DBGTRC0(debug, TRACE_GROUP, "Done");
   rpt_nl();
   return;
}


#ifdef NO
// modified from libdrm test code kms.c

static const char * const modules[] = {
   "i915",
   "radeon",
   "nouveau",
#ifdef SKIP
   "vmwgfx",
   "omapdrm",
   "exynos",
   "tilcdc",
   "msm",
   "sti",
   "tegra",
   "imx-drm",
   "rockchip",
   "atmel-hlcdc",
   "fsl-dcu-drm",
   "vc4",
#endif
   "nvidia_drm",
   "nvidia_modeset",
   "nvidia",
   NULL,
};

int util_open(const char *device, const char *module)
{
   int fd;

   if (module) {
      fd = drmOpen(module, device);
      if (fd < 0) {
         fprintf(stderr, "failed to open device '%s': %s\n",
            module, strerror(errno));
         return -errno;
      }
   } else {
      unsigned int i;

      for (i = 0; i < ARRAY_SIZE(modules); i++) {
         printf("trying to open device %s using'%s'...", device, modules[i]);

         fd = drmOpen(modules[i], device);
         if (fd < 0) {
            printf("failed\n");
         } else {
            printf("done\n");
            break;
         }
      }

      if (fd < 0) {
         fprintf(stderr, "no device found\n");
         return -ENODEV;
      }
   }

   return fd;
}
#endif


/* Examines a single DRM device, specified by name.
 *
 * Arguments:
 *   devname device name
 *   depth   logical indentation depth
 *
 * Returns:  nothing
 */
static void probe_one_device_using_libdrm(char * devname, int depth) {
   rpt_vstring(depth, "Probing device %s...", devname);

   int fd = -1;

   // drmOpen() returns a file descriptor if successful,
   // if < 0, its an errno value
   // n. errno = -fd if a passthru, but will not be set if generated internally
   //                               within drmOpen  (examined the code)
   // drmOopen() can also return DRM_ERR_NOT_ROOT (-1003)
   // DRM specific error numbers are in range -1001..-1005,
   // conflicts with our Global _Status_Code mapping

#ifdef FAIL
   fd = drmOpen(bname, NULL);
   if (fd < 0) {
      rpt_vstring(depth, "Error opening device %s using drmOpen(), fd=%s",
                         bname, linux_errno_desc(-fd));
   }
   if (fd < 0) {
      fd = drmOpen(devname, NULL);
      if (fd < 0) {
         rpt_vstring(depth, "Error opening device %s using drmOpen(), fd=%s",
                            devname, linux_errno_desc(-fd));
      }
   }
#endif

#ifdef NO
   char * busid = "pci:0000:01:00.0";
// open succeeds using hardcoded busid, but doesn't solve problem of
// driver not modesetting
// but .. if this isn't used, drmGetBusid() fails
// #ifdef WORKS_BUT_NOT_HELPFUL
   if (fd < 0) {
      fd = drmOpen(NULL, busid);
      if (fd < 0) {
         rpt_vstring(depth, "Error opening busid %s using drmOpen(), fd=%s",
                            busid, linux_errno_desc(-fd));
      }
      else {
         rpt_vstring(depth, "Successfully opened using drmOpen(NULL, \"%s\")\n",
                            busid);
      }
   }
// #endif

   if (fd < 0) {
      fd = util_open(busid, NULL);
   }
#endif

   if (fd < 0) {
      errno = 0;
      fd  = open(devname,O_RDWR | O_CLOEXEC);
      if (fd < 0) {
         rpt_vstring(depth+1, "Error opening device %s using open(), errno=%s",
                            devname, linux_errno_desc(errno));
      }
      else
         rpt_vstring(depth+1, "Open succeeded for device: %s", devname);
   }

   if (fd >= 0) {
      probe_open_device_using_libdrm(fd, depth);
      close(fd);
   }
}


/* Filter to find driN files using scandir() in get_filenames_by_filter() */
static int is_dri(const struct dirent *ent) {
   return !strncmp(ent->d_name, "card", strlen("card"));
}


/* Scans /dev/dri to obtain list of device names
 *
 * Returns:   GPtrArray of device device names.
 */
GPtrArray * get_dri_device_names_using_filesys() {
   const char *dri_paths[] = { "/dev/dri/", NULL };
   GPtrArray* dev_names = get_filenames_by_filter(dri_paths, is_dri);
   g_ptr_array_sort(dev_names, gaux_ptr_scomp);   // needed?
   return dev_names;
}


/* Main function for probing device information, particularly EDIDs,
 * using libdrm.
 *
 * 2/2017:  Nvidia's proprietary drm driver does not appear to
 * support the ioctls underlying the libdrm functions, and hence
 * the functions, set errno=22 (EINVAL).
 */
void probe_using_libdrm() {
   rpt_title("Probing connected monitors using libdrm...",0);

   if ( directory_exists("/proc/driver/nvidia/") ) {
      rpt_nl();
      rpt_vstring(1,"Checking Nvidia options to see if experimental kernel modesetting enabled:");
      char * cmd = "modprobe -c | grep \"^options nvidia\"";
      rpt_vstring(1, "Executing command: %s", cmd);
      execute_shell_cmd_rpt(cmd, 2 /* depth */);
   }

   // Check libdrm version, since there seems to be some sensitivity
   rpt_nl();
   if (is_command_in_path("pkg-config")) {
      rpt_vstring(1, "Checking libdrm version using pkg-config...");
      char * cmd = "pkg-config --modversion libdrm";
      execute_shell_cmd_rpt(cmd, 2);
   }

   else {
      // try the most common distribution specific tools

      if (is_command_in_path("dpkg-query")) {
         char * cmd = "dpkg-query -l libdrm2 | grep ii";
         rpt_vstring(1, "Checking libdrm version using dpkg-query...");
         execute_shell_cmd_rpt(cmd, 2);
      }

      rpt_nl();
      if (is_command_in_path("rpm")) {
         char * cmd = "rpm -qa | grep libdrm";
         rpt_vstring(1, "Checking libdrm version using rpm...");
         execute_shell_cmd_rpt(cmd, 2);
      }
   }

   // Examining the implementation in xf86drm.c, we see that
   // drmAvailable first calls drmOpenMinor(), then if that
   // succeeds calls drmGetVersion().  If both succeed, returns true.
   // n. drmOpenMinor() is a static function

   rpt_nl();
   // returns 1 if the DRM driver is loaded, 0 otherwise
   int drm_available = drmAvailable();
   rpt_vstring(1, "Has a DRM kernel driver been loaded? (drmAvailable()): %s",
                  bool_repr(drm_available));

#ifdef DOESNT_WORK
   // function drmOpenMinor() is static
   if (!drm_available) {
      // try the functions that drmAvailable() calls to see where the failure is
      int fd = drmOpenMinor(0, 1, DRM_MODE_PRIMARY);
      if (fd < 0) {
         rpt_vstring(1, "drmOpenMinor() failed, errno=%s", linux_errno_desc(-fd));
      }
      else {
         rpt_vstring(1, "drmOpenMinor() succeeded");
         drmVersionPtr ver = drmGetVersion(fd);
         if (ver) {
            rpt_vstring(1, "drmGetVersion() succeeded");
            drmFreeVersion(ver);
         }
         else {
            rpt_vstring(1, "drmGetVersion() failed");
         }
      }
   }
#endif

   GPtrArray * dev_names = get_dri_device_names_using_filesys();
   for (int ndx = 0; ndx < dev_names->len; ndx++) {
      char * dev_name = g_ptr_array_index(dev_names, ndx);
      rpt_nl();
      probe_one_device_using_libdrm( dev_name, 1);
   }
   g_ptr_array_free(dev_names, true);
}
