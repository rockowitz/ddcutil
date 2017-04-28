/* udev_i2c_util.c
 *
 * Created on: Apr 27, 2017
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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


#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "report_util.h"
#include "string_util.h"
#include "udev_util.h"

#include "udev_i2c_util.h"


//
// UDEV Inquiry
//
// Create, report, query, and destroy a list of summaries of UDEV I2C devices
//

// #ifdef USE_USB

/* Extract the i2c bus number from a device summary.
 *
 * Helper function for get_i2c_devices_using_udev()
 */
static
int udev_i2c_device_summary_busno(Udev_Device_Summary * summary) {
   int result = -1;
   if (str_starts_with(summary->sysname, "i2c-")) {
     const char * sbusno = summary->sysname+4;
     // DBGMSG("sbusno = |%s|", sbusno);

     int ibusno;
     bool ok = str_to_int(sbusno, &ibusno);
     if (ok)
        result = ibusno;
   }
   // DBGMSG("Returning: %d", result);
   return result;
}


/* Compare 2 Udev device summaries by their bus number
 *
 * Helper function for get_i2c_devices_using_udev(
 */
static
int compare_udev_i2c_device_summary(const void * a, const void * b) {
   Udev_Device_Summary * p1 = *(Udev_Device_Summary**) a;
   Udev_Device_Summary * p2 = *(Udev_Device_Summary**) b;

   assert( p1 && (memcmp(p1->marker, UDEV_DEVICE_SUMMARY_MARKER, 4) == 0));
   assert( p2 && (memcmp(p2->marker, UDEV_DEVICE_SUMMARY_MARKER, 4) == 0));

   int v1 = udev_i2c_device_summary_busno(p1);
   int v2 = udev_i2c_device_summary_busno(p2);

   int result = (v1 == v2) ? 0 :
                    (v1 < v2) ? -1 : 1;
   // DBGMSG("v1=%d, v2=%d, returning: %d", v1, v2, result);
   return result;
}


/* Returns array of Udev_Device_Summary for I2C devices,
 * sorted by bus number.
 */
GPtrArray * get_i2c_devices_using_udev() {
   GPtrArray * summaries = summarize_udev_subsystem_devices("i2c-dev");

   if (summaries) {
      if ( summaries->len == 0) {
         free_udev_device_summaries(summaries);   // ok if summaries == NULL
         summaries = NULL;
      }
      else {
         g_ptr_array_sort(summaries, compare_udev_i2c_device_summary);
      }
   }
   return summaries;
}


#ifdef REFERENCE
#define UDEV_DEVICE_SUMMARY_MARKER "UDSM"
typedef struct udev_device_summary {
char   marker[4];
const char * sysname;
const char * devpath;
const char * sysattr_name;
} Udev_Device_Summary;
#endif


/* Reports a collection of device summaries in table form.
 *
 * summaries       array of Udev_Device_Summary
 * title           title line
 * depth           logical indentation depth
 */
void report_i2c_device_summaries(GPtrArray * summaries, char * title, int depth) {
   rpt_vstring(0,title);
   if (!summaries || summaries->len == 0)
      rpt_vstring(depth,"No devices detected");
   else {
      rpt_vstring(depth,"%-15s %-35s %s", "Sysname", "Sysattr Name", "Devpath");
      for (int ndx = 0; ndx < summaries->len; ndx++) {
         Udev_Device_Summary * summary = g_ptr_array_index(summaries, ndx);
         assert( memcmp(summary->marker, UDEV_DEVICE_SUMMARY_MARKER, 4) == 0);
         udev_i2c_device_summary_busno(summary);   // ???
         rpt_vstring(depth,"%-15s %-35s %s",
                summary->sysname, summary->sysattr_name, summary->devpath);
      }
   }
}


#ifdef NOT_WORTH_IT
GPtrArray * get_i2c_smbus_devices_using_udev() {
   bool debug = false;
   GPtrArray * summaries = get_i2c_devices_using_udev();
   if (summaries) {
      for (int ndx = summaries->len-1; ndx >= 0; ndx--) {
         Udev_Device_Summary * summary = g_ptr_array_index(summaries, ndx);
         assert(memcmp(summary->marker, UDEV_DEVICE_SUMMARY_MARKER, 4) == 0);
         if ( !str_starts_with(summary->sysattr_name, "SMBus") ) {
            // TODO: g_ptr_array_set_free_function() must already have been called
            g_ptr_array_remove_index(summaries, ndx);
         }
      }
   }

   if (debug)
      report_i2c_device_summaries(summaries, "I2C SMBus Devices:", 0);


   return summaries;
}
#endif


/* Given a specified I2C bus number, checks the list of I2C device
 * summaries to see if it is the bus number of a SMBUS device.
 *
 * Arguments;
 *    summaries    array of Udev_Device_Summary
 *    sbusno       I2C bus number, as string
 *
 * Returns:
 *    true if the number is that of an SMBUS device, false otherwise
 */
bool is_smbus_device_summary(GPtrArray * summaries, char * sbusno) {
   bool debug = false;
   char devname [10];
   snprintf(devname, sizeof(devname), "i2c-%s", sbusno);
   // DBGMSF(debug, "sbusno=|%s|, devname=|%s|", sbusno, devname);
   if (debug)
      printf("(%s) sbusno=|%s|, devname=|%s|\n", __func__, sbusno, devname);
   bool result = false;
   for (int ndx = 0; ndx < summaries->len; ndx++) {
      Udev_Device_Summary * summary = g_ptr_array_index(summaries, ndx);
      if ( streq(summary->sysname, devname) &&
           str_starts_with(summary->sysattr_name, "SMBus") )
      {
         result = true;
         break;
      }
   }
   // DBGMSF(debug, "Returning: %s", bool_repr(result), result);
   if (debug)
      printf("(%s) Returning: %s", __func__, bool_repr(result));
   return result;
}


Byte_Value_Array get_i2c_devices_as_bva_using_udev() {
   bool debug = false;
   if (debug)
      printf("(%s) Starting.\n", __func__);

   Byte_Value_Array bva = bva_create();

   GPtrArray * summaries = get_i2c_devices_using_udev();
   if (summaries) {
      for (int ndx = 0; ndx < summaries->len; ndx++) {
         Udev_Device_Summary * summary = g_ptr_array_index(summaries, ndx);
         if ( str_starts_with(summary->sysattr_name, "SMBus") )
            continue;
         int busno = udev_i2c_device_summary_busno(summary);
         assert(busno >= 0);
         assert(busno <= 127);
         bva_append(bva, busno);
      }
      g_ptr_array_free(summaries, true);
   }

   if (debug)
      bva_report(bva, "Returning I2c bus numbers:");
   return bva;
}
