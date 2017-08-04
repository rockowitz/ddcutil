/* query_sysenv_xref.c
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
 * Display identifier cross-reference
 */

/** \cond */
#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
/** \endcond */

#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"

#include "query_sysenv_xref.h"


/** Collection of Device_Id_Xref */
static GPtrArray * device_xref = NULL;

void device_xref_init() {
   device_xref = g_ptr_array_new();
}

/** Finds an existing cross-reference entry with the specified
 *  128 byte EDID value.
 *
 *  \param  raw_edid pointer to 128 byte EDID
 *  \return pointer to existing #Device_Id_Xref,\n
 *          NULL if not found
 */
Device_Id_Xref * device_xref_find(Byte * raw_edid) {
   Device_Id_Xref * result = NULL;
   for (int ndx = 0; ndx < device_xref->len; ndx++) {
      Device_Id_Xref * cur = g_ptr_array_index(device_xref, ndx);
      assert( memcmp(cur->marker, DEVICE_ID_XREF_MARKER, 4) == 0);
      if ( memcmp(cur->raw_edid, raw_edid, 128) == 0 ) {
         result = cur;
         break;
      }
   }
   return result;
}


/** Creates a new #Device_Id_Xref with the specified EDID value.
 *
 *  \param  raw_edid pointer 128 byte EDID
 *  \return pointer to newly allocated #Device_Id_Xref
 */
Device_Id_Xref * device_xref_new(Byte * raw_edid) {
   Device_Id_Xref * xref = calloc(1, sizeof(Device_Id_Xref));
   memcpy(xref->marker, DEVICE_ID_XREF_MARKER, 4);
   memcpy(xref->raw_edid, raw_edid, 128);
   xref->edid_tag =  hexstring2(xref->raw_edid+124, 4, NULL, true, NULL, 0);
   xref->i2c_busno = -1;
#ifdef ALTERNATIVE
   xref->sysfs_drm_busno = -1;
#endif
   // DBGMSG("Created xref %p with tag: %s", xref, xref->edid_tag);
   return xref;
}


/** Returns the #Device_Id_Xref for the specified EDID value.
 *  If the #Device_Id_Xref does not already exist, it is created
 *
 *  \param  raw_edid pointer 128 byte EDID
 *  \return pointer to #Device_Id_Xref
 */
Device_Id_Xref * device_xref_get(Byte * raw_edid) {
   if (!device_xref)
      device_xref_init();

   Device_Id_Xref * xref = device_xref_find(raw_edid);
   if (!xref) {
      xref = device_xref_new(raw_edid);
      g_ptr_array_add(device_xref, xref);
   }
   // else
   //    DBGMSG("Found Device_Id_Xref %p for edid ...%s", xref, xref->edid_tag);
   return xref;
}


/** Find the #Device_Id_Xref for the specified I2C bus number
 *
 * \param  busno  I2C bus number
 * \return device identification cross-reference entry,\n
 *         NULL if not found
 *
 */
Device_Id_Xref * device_xref_find_by_busno(int busno) {
   bool debug = false;

   Device_Id_Xref * result = NULL;
   for (int ndx = 0; ndx < device_xref->len; ndx++) {
      Device_Id_Xref * cur = g_ptr_array_index(device_xref, ndx);
      assert( memcmp(cur->marker, DEVICE_ID_XREF_MARKER, 4) == 0);
      if ( cur->i2c_busno == busno) {
         result = cur;
         break;
      }
   }
   if (debug) {
      if (result)
         DBGMSG("busno = %d, returning Device_Id_Xref %p for EDID ...%s", busno, result, result->edid_tag);
      else
         DBGMSG("busno = %d, not found", busno);
   }
   return result;
}


/** Reports the device identification cross-reference table.
 *
 * \param depth logical indentation depth
 */
void device_xref_report(int depth) {
   int d1 = depth+1;
   int d2 = depth+2;

   rpt_nl();
   rpt_vstring(depth, "Device Identifier Cross Reference Report");
   // rpt_nl();

   // rpt_vstring(d1, "EDID    Busno Xrandr  DRM   Udev name   Udev Path");

   for (int ndx = 0; ndx < device_xref->len; ndx++) {
      Device_Id_Xref * xref = g_ptr_array_index(device_xref, ndx);
      assert( memcmp(xref->marker, DEVICE_ID_XREF_MARKER, 4) == 0);

      Parsed_Edid * parsed_edid = create_parsed_edid(xref->raw_edid);

#ifdef NO
      rpt_vstring(d1, "...%s  %2d %-10s %-10s %-30s %s",
                  xref->edid_tag,
                  xref->i2c_busno,
                  xref->xrandr_name,
                  xref->drm_connector_name,
                  xref->udev_name,
                  xref->udev_syspath);
      if (parsed_edid) {
         rpt_vstring(d2, "Mfg:   %s", parsed_edid->mfg_id);
         rpt_vstring(d2, "Model: %s", parsed_edid->model_name);
         rpt_vstring(d2, "SN:    %s", parsed_edid->serial_ascii);
      }
#endif

      rpt_nl();
      if (parsed_edid) {
         rpt_vstring(d1, "EDID: ...%s  Mfg: %-3s  Model: %-13s  SN: %-13s",
                         xref->edid_tag,
                         parsed_edid->mfg_id,
                         parsed_edid->model_name,
                         parsed_edid->serial_ascii);
         free_parsed_edid(parsed_edid);
      }
      else
         rpt_vstring(d1, "EDID: ...%s", xref->edid_tag);

      if (xref->i2c_busno == -1)
         rpt_vstring(d2, "I2C device:    Not found");
      else
         rpt_vstring(d2, "I2C device:     /dev/i2c-%d", xref->i2c_busno);
      rpt_vstring(d2, "XrandR output:  %s", xref->xrandr_name);
      rpt_vstring(d2, "DRM connector:  %s", xref->drm_connector_name);
      // rpt_vstring(d2, "DRM path:       %s", xref->drm_device_path);
      rpt_vstring(d2, "UDEV name:      %s", xref->udev_name);
      rpt_vstring(d2, "UDEV syspath:   %s", xref->udev_syspath);
      rpt_vstring(d2, "sysfs drm path: %s", xref->sysfs_drm_name);

      // pick one way or the other
      rpt_vstring(d2, "sysfs drm I2C:  %s", xref->sysfs_drm_i2c);
#ifdef ALTERNATIVE
      if (xref->sysfs_drm_busno == -1)
         rpt_vstring(d2, "sysfs drm bus: Not found");
      else
         rpt_vstring(d2, "sysfs drm bus: i2c-%d", xref->sysfs_drm_busno);
#endif

      // TEMP to screen scrape the EDID:
      // if (xref->raw_edid) {
      //    char * s = hexstring2(xref->raw_edid, 128,
      //                  NULL, true, NULL, 0);
      //    puts(s);
      //    free(s);
      // }
   }
}

