/* udev_i2c_util.c
 *
 * <copyright>
 * Copyright (C) 2016-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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
 * I2C specific udev utilities
 */

/** \cond */
#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
/** \endcond */

#include "report_util.h"
#include "string_util.h"
#include "udev_util.h"

#include "udev_i2c_util.h"


//
// UDEV Inquiry
//
// Create, report, query, and destroy a list of summaries of UDEV I2C devices
//

#ifdef REFERENCE
#define UDEV_DEVICE_SUMMARY_MARKER "UDSM"
typedef struct udev_device_summary {
   char   marker[4];
   const char * sysname;
   const char * devpath;
   const char * sysattr_name;
} Udev_Device_Summary;
#endif


/* Extract the i2c bus number from a device summary.
 *
 * Helper function for get_i2c_devices_using_udev()
 *
 * \param pointer to Udev_Device_Summary
 * \return I2C bus number from sysname attribute, -1 if unable to parse
 */
int
udev_i2c_device_summary_busno(Udev_Device_Summary * summary) {
   int result = -1;
   // On Raspbian, not all sysattr names have the form i2c-n.
   if (str_starts_with(summary->sysname, "i2c-")) {
     const char * sbusno = summary->sysname+4;
     // DBGMSG("sbusno = |%s|", sbusno);

     int ibusno;
     bool ok = str_to_int(sbusno, &ibusno);
     if (ok)
        result = ibusno;
   }
   // DBGMSG("sysname=|%s|. Returning: %d", summary->sysname, result);
   return result;
}


/* Compare 2 udev device summaries of I2C devices by their I2C bus number
 *
 * Helper function for get_i2c_devices_using_udev()
 */
static int
compare_udev_i2c_device_summary(const void * a, const void * b) {
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


/** Returns array of Udev_Device_Summary for I2C devices found by udev,
 *  sorted by bus number.
 *
 * \return array of #Udev_Device_Summary
 */
GPtrArray *
get_i2c_devices_using_udev() {
   GPtrArray * summaries = summarize_udev_subsystem_devices("i2c-dev");
   assert(summaries);

   if (summaries->len > 1) {
      g_ptr_array_sort(summaries, compare_udev_i2c_device_summary);
   }
   return summaries;
}


/** Reports a collection of #Udev_Device_Summary for I2C devices in table form.
 *
 * summaries       array of #Udev_Device_Summary
 * title           title line
 * depth           logical indentation depth
 */
void
report_i2c_udev_device_summaries(GPtrArray * summaries, char * title, int depth) {
   rpt_vstring(0,title);
   if (!summaries || summaries->len == 0)
      rpt_vstring(depth,"No devices detected");
   else {
      rpt_vstring(depth,"%-11s %-10s %-35s %s", "Subsystem", "Sysname", "Sysattr Name", "Devpath");
      for (int ndx = 0; ndx < summaries->len; ndx++) {
         Udev_Device_Summary * summary = g_ptr_array_index(summaries, ndx);
         assert( memcmp(summary->marker, UDEV_DEVICE_SUMMARY_MARKER, 4) == 0);
         // udev_i2c_device_summary_busno(summary);   // ???
         rpt_vstring(depth,"%-11s %-10s %-35s %s",
                summary->subsystem, summary->sysname, summary->sysattr_name, summary->devpath);
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
      report_i2c_udev_device_summaries(summaries, "I2C SMBus Devices:", 0);


   return summaries;
}
#endif


#ifdef UNUSED
/** Given a specified I2C bus number, checks a list of I2C device
 *  summaries to see if it is the bus number of a SMBUS device.
 *
 *  \param  summaries    array of Udev_Device_Summary
 *  \param  sbusno       I2C bus number, as string
 *
 *  \return  **true** if the number is that of an SMBUS device, **false* if not
 */
bool
is_smbus_device_summary(GPtrArray * summaries, char * sbusno) {
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
#endif

/** Gets the numbers of all non-SMBus I2C devices
 *
 *  \param  include_smbus  if true, do not exclude SMBus devices
 *  \return sorted #Byte_Value_Array of I2C device numbers
 */
Byte_Value_Array
get_i2c_device_numbers_using_udev(bool include_smbus) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting.\n", __func__);

   Byte_Value_Array bva = bva_create();

   GPtrArray * summaries = get_i2c_devices_using_udev();
   if (summaries) {
      for (int ndx = 0; ndx < summaries->len; ndx++) {
         Udev_Device_Summary * summary = g_ptr_array_index(summaries, ndx);
         if ( !include_smbus && str_starts_with(summary->sysattr_name, "SMBus") )
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


/** Gets the bus numbers of I2C devices reported by UDEV,
 *  optionally filtered by the sys attribute name (e.g. to eliminate SMBus devices).
 *
 *  \param  keep_func predicate function
 *  \return sorted #Byte_Value_Array of I2C device numbers
 *
 *  \remark
 *  ***keep_func*** takes a bus number as its sole argument,
 *  returning true iff the number should be included
 *  \remark,
 *  if the udev sysname value does not have the form i2c-n,
 *  the udev node is ignored
 *  \remark
 *  if ***keep_func*** is NULL, all device numbers
 *  are included, i.e. there is no filtering
 */
Byte_Value_Array
get_i2c_device_numbers_using_udev_w_sysattr_name_filter(Sysattr_Name_Filter keep_func) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting.\n", __func__);

   Byte_Value_Array bva = bva_create();

   GPtrArray * summaries = get_i2c_devices_using_udev();
   if (summaries) {
      for (int ndx = 0; ndx < summaries->len; ndx++) {
         Udev_Device_Summary * summary = g_ptr_array_index(summaries, ndx);
         bool keep = true;
         if (keep_func)
            keep = keep_func(summary->sysattr_name);
         if (keep) {
            int busno = udev_i2c_device_summary_busno(summary);
            if (busno >= 0) {      // -1 if parse error
               assert(busno <= 127);
               bva_append(bva, busno);
            }
         }
      }
      g_ptr_array_free(summaries, true);
   }

   if (debug)
      bva_report(bva, "Returning I2c bus numbers:");
   return bva;
}

