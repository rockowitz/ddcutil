/* query_drm_sysenv.c
 *
 * <copyright>
 * Copyright (C) 2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

// ??
// define GNU_SOURCE

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

#include "base/core.h"
#include "base/linux_errno.h"

#include "query_drm_sysenv.h"

#ifdef REF
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

char * drm_bus_type_name(uint8_t bus) {
   char * result = NULL;
   if (bus == DRM_BUS_PCI)
      result = "pci";
   else
      result = "unk";
   return result;
}


void report_drmVersion(drmVersion * vp, int depth) {
   rpt_vstring(depth, "Version:     %d.%d.%d",
                      vp->version_major, vp->version_minor, vp->version_patchlevel);
   rpt_vstring(depth, "Driver:      %.*s", vp->name_len, vp->name);
   rpt_vstring(depth, "Date:        %.*s", vp->date_len, vp->date);
   rpt_vstring(depth, "Description: %.*s", vp->desc_len, vp->desc);
}


static void probe_open_device_using_libdrm(int fd, int depth) {
   int d1 = depth+1;
   int d2 = depth+2;
   int d3 = depth+3;
   bool debug = false;
   int rc;

   rpt_nl();

   drmVersionPtr vp = drmGetVersion(fd);
   if (vp) {
      rpt_vstring(d1, "Driver version information:");
      report_drmVersion(vp, d2);
      drmFreeVersion(vp);
   }
   else {
      rpt_vstring(d1, "Error calling drmGetVersion().  errno=%s", linux_errno_desc(errno));
   }

   vp = drmGetLibVersion(fd);
   if (vp) {
      rpt_vstring(d1, "DRM library version information:");
      report_drmVersion(vp, d2);
      drmFreeVersion(vp);
   }
   else {
      rpt_vstring(d1, "Error calling drmGetLibVersion().  errno=%s", linux_errno_desc(errno));
   }

   char * bufid = drmGetBusid(fd);
   if (bufid) {
      rpt_vstring(d1, "DRM Busid:  %s");
      drmFreeBusid(bufid);
   }
   else {
      rpt_vstring(d1, "Error calling drmGetBusid().  errno=%s", linux_errno_desc(errno));
   }

   rpt_nl();
   struct _drmDevice * ddev;
   rc = drmGetDevice(fd, &ddev);
   if (rc < 0) {
      rpt_vstring(depth, "Error calling drmGetDevice, errno=%s", linux_errno_desc(errno));
   }
   else {
      rpt_vstring(d1, "Device information:");
      rpt_vstring(d2, "bustype:             %d - %s",
            ddev->bustype, drm_bus_type_name(ddev->bustype));
#ifdef OLD
      rpt_vstring(d2, "bus,dev,domain,func: %d, %d, %d, %d",
            ddev->businfo.pci->bus,
            ddev->businfo.pci->dev,
            ddev->businfo.pci->domain,
            ddev->businfo.pci->func);
#endif
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
         else
            summarize_drm_modeProperty(prop_ptr, d2);

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

      rpt_vstring(d1, "%-20s %u",       "connector_id:",      conn->connector_id);
      rpt_vstring(d2, "%-20s %d - %s",  "connector_type:",    conn->connector_type,  connector_type_name(conn->connector_type));
      rpt_vstring(d2, "%-20s %d",       "connector_type_id:", conn->connector_type_id);
      rpt_vstring(d2, "%-20s %d - %s",  "connection:", conn->connection, connector_status_name(conn->connection));
      uint32_t encoder_id = conn->encoder_id;     // current encoder
      rpt_vstring(d2, "%-20s %d",       "encoder:", encoder_id);

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
                  if (parsed_edid)
                     report_parsed_edid(parsed_edid, true /* verbose */ , d3);
               }

               drmModeFreePropertyBlob(blob_ptr);
            }
         }
         else if (conn->props[ndx] == subconnector_prop_id) {
            uint32_t enum_value = conn->prop_values[ndx];
            // printf("subconnector value: %d\n", enum_value);

            // assert(subconn_prop_ptr->flags & DRM_MODE_PROP_ENUM);
            // assert(enum_value < subconn_prop_ptr->count_enums);

            if (subconn_prop_ptr->flags & DRM_MODE_PROP_ENUM) {
               bool found = false;
                for (int i = 0; i < subconn_prop_ptr->count_enums && !found; i++) {
                   if (subconn_prop_ptr->enums[i].value == enum_value) {
                      rpt_vstring(d2, "Subconnector value = %d - %s", enum_value, subconn_prop_ptr->enums[i].name);
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

   }

   if (edid_prop_ptr) {
      drmModeFreeProperty(edid_prop_ptr);
   }
   if (subconn_prop_ptr) {
      drmModeFreeProperty(subconn_prop_ptr);
   }

bye:
   return;
}


static void probe_one_device_using_libdrm(char * devname, int depth) {
   rpt_vstring(depth, "Probing device %s...", devname);

   int supported = drmCheckModesettingSupported(devname);
   DBGMSG("drmCheckModesettingSupported() returned %d", supported);

   int fd  = open(devname,O_RDWR | O_CLOEXEC);
   if (fd < 0) {
      rpt_vstring(depth, "Error opening device %s, errno=%s",
                         devname, linux_errno_desc(errno));
      // perror("Error opening %s\n");
   }
   else {
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
   g_ptr_array_sort(dev_names, g_ptr_scomp);   // needed?
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

   int drm_available = drmAvailable();
   rpt_vstring(0, "drmAvailable() returned %d", drm_available);

   GPtrArray * dev_names = get_dri_device_names_using_filesys();
   for (int ndx = 0; ndx < dev_names->len; ndx++) {
      char * dev_name = g_ptr_array_index(dev_names, ndx);
      rpt_nl();
      probe_one_device_using_libdrm( dev_name, 0);
   }
   g_ptr_array_free(dev_names, true);
}

