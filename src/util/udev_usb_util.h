/* udev_usb_util.h
 *
 * <copyright>
 * Copyright (C) 2016-2021 Sanford Rockowitz <rockowitz@minsoft.com>
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
  * USB specific udev utility functions
  */

#ifndef UDEV_USB_UTIL_H_
#define UDEV_USB_UTIL_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <libudev.h>
#include <stdint.h>
/** \endcond */

void probe_udev_subsystem(char * udev_class, bool show_usb_parent, int depth);


#define UDEV_DETAILED_DEVICE_SUMMARY_MARKER "UDDS"
/** Identifying information for UDEV device
 *
 * @remark
 * Currently (3/2017) used solely for informational messages,
 * so no need to convert from strings to integers.
 */
typedef struct {
   char      marker[4];        ///< always "UDDS"
   char *    devname;          ///< e.g. /dev/usb/hiddev2
   // int       usb_busnum;
   // int       usb_devnum;
   // uint16_t  vid;
   // uint16_t  pid;
   char *    vendor_id;       ///< vendor id, as 4 hex characters
   char *    product_id;      ///< product id, as 4 hex characters
   char *    vendor_name;     ///< vendor name
   char *    product_name;    ///< product name
   char *    busnum_s;        ///< bus number, as a string
   char *    devnum_s;        ///< device number, as a string

   // to collect, then reduce to what's needed:
   char *    prop_busnum  ;
   char *    prop_devnum  ;
   char *    prop_model  ;
   char *    prop_model_id  ;
   char *    prop_usb_interfaces  ;
   char *    prop_vendor  ;
   char *    prop_vendor_from_database  ;
   char *    prop_vendor_id  ;
   char *    prop_major ;
   char *    prop_minor  ;
} Usb_Detailed_Device_Summary;

void free_usb_detailed_device_summary(Usb_Detailed_Device_Summary * devsum);
void report_usb_detailed_device_summary(Usb_Detailed_Device_Summary * devsum, int depth);
Usb_Detailed_Device_Summary * lookup_udev_usb_device_by_devname(
                                 const char * devname,
                                 bool verbose);

/** USB bus number/device number pair */
typedef struct udev_usb_devinfo {
   uint16_t busno;   ///< USB bus number
   uint16_t devno;   ///< device number on USB bus
} Udev_Usb_Devinfo;

void report_udev_usb_devinfo(struct udev_usb_devinfo * dinfo, int depth);
Udev_Usb_Devinfo * get_udev_usb_devinfo(char * subsystem, char * simple_devname);

// Move function to hiddev utility library?
/** Excapsulates location of hiddev device files, in case it needs to be generalized */
char * usb_hiddev_directory();


#endif /* UDEV_USB_UTIL_H_ */
