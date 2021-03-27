/** @file query_sysenv.xref.h
 *
 *  Table cross-referencing the multiple ways that a display is referenced
 *  in various Linux subsystems.
 */

// Copyright (C) 2017-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */

#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdio.h>
#include <string.h>

#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"
/** \endcond */

#include "query_sysenv_xref.h"


/** Collection of #Device_Id_Xref */
static GPtrArray * device_xref = NULL;
static bool i2c_bus_scan_complete = false;


/** Initializes the device cross reference table */
void device_xref_init() {
   device_xref = g_ptr_array_new();
}


static void device_xref_mark_duplicate_edid(Byte * raw_edid) {
   assert(i2c_bus_scan_complete);
   for (int ndx = 0; ndx < device_xref->len; ndx++) {
      Device_Id_Xref * cur = g_ptr_array_index(device_xref, ndx);
      assert( memcmp(cur->marker, DEVICE_ID_XREF_MARKER, 4) == 0);
      if ( memcmp(cur->raw_edid, raw_edid, 128) == 0 ) {
         cur->ambiguous_edid = true;
      }
   }
}


static int device_xref_count_by_edid(Byte * raw_edid) {
   int ct = 0;
   assert(i2c_bus_scan_complete);
   for (int ndx = 0; ndx < device_xref->len; ndx++) {
      Device_Id_Xref * cur = g_ptr_array_index(device_xref, ndx);
      assert( memcmp(cur->marker, DEVICE_ID_XREF_MARKER, 4) == 0);
      if ( memcmp(cur->raw_edid, raw_edid, 128) == 0 ) {
           ct++;
      }
   }
   return ct;
}


// A truly dumb algorithm, but the size of device_xref is tiny
static void device_xref_mark_duplicate_edids() {
   int ct = device_xref->len;
   for (int start = 0; start < ct-1; start++) {
      Device_Id_Xref * cur_xref = g_ptr_array_index(device_xref, start);
      int dupct = device_xref_count_by_edid(cur_xref->raw_edid);
      if (dupct > 1) {
         device_xref_mark_duplicate_edid(cur_xref->raw_edid);
      }
   }
}

/** Indicates that scanning by I2C device number is complete,
 *  and triggers check for duplicate EDIDs.
 */
void device_xref_set_i2c_bus_scan_complete() {
   bool debug = false;
   // DBGMSG("Setting i2c_bus_scan_complete");
   i2c_bus_scan_complete = true;
   device_xref_mark_duplicate_edids();
   if (debug) {
      DBGMSG("After checking for duplicate EDIDs:");
      device_xref_report(3);
   }
}

#ifdef UNUSED
bool device_xref_i2c_bus_scan_complete() {
   return i2c_bus_scan_complete;
}
#endif



/** Finds an existing cross-reference entry with the specified
 *  128 byte EDID value.
 *
 *  \param  raw_edid pointer to 128 byte EDID
 *  \return pointer to existing #Device_Id_Xref,\n
 *          NULL if not found
 *
 *  \remark If multiple monitors have the same EDID (e.x. identical LG display monitors)
 *          returns the first entry in the cross-reference list
 */
Device_Id_Xref * device_xref_find_by_edid(Byte * raw_edid) {
   assert(i2c_bus_scan_complete);
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


/** Returns the last 4 bytes of a 128-byte EDID as a
 *  hexadecimal string.
 *
 *  @param   raw_edid pointer to EDID
 *  @return  bytes 124..127 as a string, caller must free
 */
char * device_xref_edid_tag(Byte * raw_edid) {
   return  hexstring2(raw_edid+124, 4, NULL, true, NULL, 0);
}


/** Creates a new #Device_Id_Xref with the specified EDID value.
 *
 *  \param  raw_edid pointer 128 byte EDID
 *  \return pointer to newly allocated #Device_Id_Xref
 *
 *  \todo merge into #device_xref_new_with_busno()
 */
static Device_Id_Xref * device_xref_new(Byte * raw_edid) {
   Device_Id_Xref * xref = calloc(1, sizeof(Device_Id_Xref));
   memcpy(xref->marker, DEVICE_ID_XREF_MARKER, 4);
   memcpy(xref->raw_edid, raw_edid, 128);
   xref->edid_tag =  device_xref_edid_tag(xref->raw_edid);
   xref->i2c_busno = -1;
// #ifdef ALTERNATIVE
   xref->sysfs_drm_busno = -1;
// #endif
   // DBGMSG("Created xref %p with tag: %s", xref, xref->edid_tag);
   return xref;
}

#ifdef OLD
/** Returns the #Device_Id_Xref for the specified EDID value.
 *  If the #Device_Id_Xref does not already exist, it is created
 *
 *  \param  raw_edid pointer 128 byte EDID
 *  \return pointer to #Device_Id_Xref
 */
Device_Id_Xref * device_xref_get(Byte * raw_edid) {
   if (!device_xref)
      device_xref_init();

   Device_Id_Xref * xref = device_xref_find_by_edid(raw_edid);
   if (!xref) {
      xref = device_xref_new(raw_edid);
      g_ptr_array_add(device_xref, xref);
   }
   // else
   //    DBGMSG("Found Device_Id_Xref %p for edid ...%s", xref, xref->edid_tag);
   return xref;
}
#endif


/** Find the #Device_Id_Xref for the specified I2C bus number
 *
 * \param  busno  I2C bus number
 * \return device identification cross-reference entry,\n
 *         NULL if not found
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


/** Creates a new #Device_Id_Xref with the specified bus number and EDID value.
 *
 *  \param  busno     I2C bus number
 *  \param  raw_edid  pointer to 128 byte EDID
 *  \return pointer to newly allocated #Device_Id_Xref
 */
Device_Id_Xref * device_xref_new_with_busno(int busno, Byte * raw_edid) {
   assert(busno >= 0);
   assert(raw_edid);

   bool debug = false;

   Device_Id_Xref * xref = NULL;
   xref = device_xref_find_by_busno(busno);
   assert(!xref);

   xref = device_xref_new(raw_edid);
   xref->i2c_busno = busno;
   xref->udev_busno = -1;
   g_ptr_array_add(device_xref, xref);
   DBGMSF(debug, "Created xref %p with busno %d, EDID tag: ...%s",
                 xref, xref->i2c_busno, xref->edid_tag);
   return xref;
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
      rpt_vstring(d1, "/dev/i2c busno:     %d", xref->i2c_busno);

      if (parsed_edid) {
         rpt_vstring(d2, "EDID: ...%s  Mfg: %-3s  Model: %-13s  SN: %-13s",
                         xref->edid_tag,
                         parsed_edid->mfg_id,
                         parsed_edid->model_name,
                         parsed_edid->serial_ascii);
         rpt_vstring(d2, "                   Product number: %d, binary SN: %d",
                         parsed_edid->product_code, parsed_edid->serial_binary);

         free_parsed_edid(parsed_edid);
      }
      else
         rpt_vstring(d2, "EDID: ...%s", xref->edid_tag);

      // if (xref->i2c_busno == -1)
      //    rpt_vstring(d2, "/dev/i2c busno:     Not found");
      // else
      //    rpt_vstring(d2, "/dev/i2c busno:     %d", xref->i2c_busno);
      rpt_vstring(d2, "XrandR output:      %s", xref->xrandr_name);
      rpt_vstring(d2, "DRM connector:      %s", xref->drm_connector_name);
      // rpt_vstring(d2, "DRM path:        %s", xref->drm_device_path);
      rpt_vstring(d2, "UDEV name:          %s", xref->udev_name);
      rpt_vstring(d2, "UDEV syspath:       %s", xref->udev_syspath);
      rpt_vstring(d2, "UDEV busno:         %d", xref->udev_busno);
      rpt_vstring(d2, "sysfs drm path:     %s", xref->sysfs_drm_name);

      // pick one way or the other
      rpt_vstring(d2, "sysfs drm I2C:      %s", xref->sysfs_drm_i2c);
// #ifdef ALTERNATIVE
      if (xref->sysfs_drm_busno == -1)
         rpt_vstring(d2, "sysfs drm busno:    Unknown");
      else
         rpt_vstring(d2, "sysfs drm busno:    %d", xref->sysfs_drm_busno);
// #endif
      rpt_vstring(d2, "ambiguous EDID:     %s", sbool(xref->ambiguous_edid));
      if (xref->ambiguous_edid) {
         rpt_vstring(d2, "WARNING: Multiple displays have same EDID. XrandR and DRM values may be incorrect");
      }
      // TEMP to screen scrape the EDID:
      // if (xref->raw_edid) {
      //    char * s = hexstring2(xref->raw_edid, 128,
      //                  NULL, true, NULL, 0);
      //    puts(s);
      //    free(s);
      // }
   }
}

