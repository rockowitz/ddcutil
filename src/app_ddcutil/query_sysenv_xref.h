/* query_sysenv_xref.h
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

/** \file
 *
 */

#ifndef QUERY_SYSENV_XREF_H_
#define QUERY_SYSENV_XREF_H_

#include "util/edid.h"

#define DEVICE_ID_XREF_MARKER "DXRF"
/** Device identifier cross-reference entry */
typedef struct {
   char          marker[4];
   Byte          raw_edid[128];
   char *        edid_tag;
   Parsed_Edid * parsed_edid;
   int           i2c_busno;
   char *        xrandr_name;
   char *        udev_name;
   char *        udev_syspath;
   char *        drm_connector_name;
   int           drm_connector_type;
   char *        drm_device_path;
   char *        sysfs_drm_name;
   char *        sysfs_drm_i2c;     // or save I2C bus number found?
#ifdef ALTERNATIVE
   int           sysfs_drm_busno;
#endif
} Device_Id_Xref;

void device_xref_init();

Device_Id_Xref * device_xref_get(Byte * raw_edid);
Device_Id_Xref * device_xref_find_by_busno(int busno);
void device_xref_report(int depth);


#endif /* QUERY_SYSENV_XREF_H_ */
