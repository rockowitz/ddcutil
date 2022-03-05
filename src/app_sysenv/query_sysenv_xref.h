/** @file query_sysenv.xref.h
 *
 *  Table cross-referencing the multiple ways that a display is referenced
 *  in various Linux subsystems.
 */

// Copyright (C) 2017-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef QUERY_SYSENV_XREF_H_
#define QUERY_SYSENV_XREF_H_

#include "util/edid.h"

#define DEVICE_ID_XREF_MARKER "DXRF"
/** Device identifier cross-reference entry */
typedef struct {
   char          marker[4];

   // Subsystem ids:
   //   I2C  scan by I2C bus number
   //   DRM
   //   SYSFS query /sys
   //   X11   query X11
   //   UDEV  query udev


   Byte          raw_edid[128];      // All   DRM   I2C   SYSFS   X11
   char *        edid_tag;
   Parsed_Edid * parsed_edid;        // All   DRM   I2C   SUSFS
   int           i2c_busno;          //             I2C
   char *        xrandr_name;        //                           X11
   char *        udev_name;          //                                 UDEV
   char *        udev_syspath;       //                                 UDEV
   int           udev_busno;         //                                 UDEV
   char *        drm_connector_name; //       DRM
   int           drm_connector_type; //       DRM
   char *        drm_device_path;    //       DRM
   char *        sysfs_drm_name;     //                   SYSFS
   char *        sysfs_drm_i2c;     //                    SYSFS  // or save I2C bus number found?
// #ifdef ALTERNATIVE
   int           sysfs_drm_busno;
// #endif
   bool          ambiguous_edid;
} Device_Id_Xref;

void device_xref_init();
void device_xref_set_i2c_bus_scan_complete();
// bool device_xref_i2c_bus_scan_complete();

char * device_xref_edid_tag(const Byte * raw_edid);

Device_Id_Xref * device_xref_find_by_edid(const Byte * raw_edid);
Device_Id_Xref * device_xref_find_by_busno(int busno);
Device_Id_Xref * device_xref_new_with_busno(int busno, Byte * raw_edid);
void device_xref_report(int depth);

#endif /* QUERY_SYSENV_XREF_H_ */
