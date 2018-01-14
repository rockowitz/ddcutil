/* libusb_reports.h
 *
 * libusb is not currently used by ddcutil.  This code is retained for reference.
 *
 * <copyright>
 * Copyright (C) 2016-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

// Adapted from usbplay2 file libusb_util.h

#ifndef LIBUSB_REPORTS_H_
#define LIBUSB_REPORTS_H_

#include <libusb-1.0/libusb.h>      // need pkgconfig?
#include <stdbool.h>
// #include <wchar.h>

#include "util/coredefs.h"


#define LIBUSB_EXIT     true
#define LIBUSB_CONTINUE false


#define REPORT_LIBUSB_ERROR(_funcname, _errno, _exit_on_error) \
   do { \
      printf("(%s) " _funcname " returned %d (%s): %s\n", \
             __func__, \
             _errno,   \
             libusb_error_name(_errno), \
             libusb_strerror(_errno) \
            ); \
      if (_exit_on_error) \
         exit(1); \
   } while(0);

#define CHECK_LIBUSB_RC(_funcname, _rc, _exit_on_error) \
   do { \
      if (_rc < 0) { \
         printf("(%s) " _funcname " returned %d (%s): %s\n", \
                __func__, \
                _rc,   \
                libusb_error_name(_rc), \
                libusb_strerror(_rc) \
               ); \
         if (_exit_on_error) \
            exit(1); \
      } \
   } while(0);


// Initialization

void init_libusb_reports();  // initialize lookup tables


// Lookup descriptive names of constants

char * descriptor_title(Byte val);
char * endpoint_direction_title(Byte val);
char * transfer_type_title(Byte val);
char * class_code_title(Byte val);

// Misc utilities

char *    lookup_libusb_string(     struct libusb_device_handle * dh, int string_id);
#ifdef UNUSED
wchar_t * lookup_libusb_string_wide(struct libusb_device_handle * dh, int string_id);
#endif

// Report functions for libusb data structures

void report_libusb_endpoint_descriptor(
        const struct libusb_endpoint_descriptor *  epdesc,
        libusb_device_handle *                     dh,    // may be null
        int                                        depth);
void report_libusb_interface_descriptor(
        const struct libusb_interface_descriptor * inter,
        libusb_device_handle *                     dh,    // may be null
        int                                        depth);
void report_libusb_interface(
        const struct libusb_interface *            interface,
        libusb_device_handle *                     dh,    // may be null
        int                                        depth) ;
void report_libusb_config_descriptor(
        const struct libusb_config_descriptor *    config,
        libusb_device_handle *                     dh,    // may be null
        int                                        depth);
void report_libusb_device_descriptor(
        const struct libusb_device_descriptor *    desc,
        libusb_device_handle *                     dh,    // may be null
        int                                        depth);
void report_libusb_device(
        libusb_device *                            dev,
        bool                                       show_hubs,
        int                                        depth);

void report_libusb_devices(
        libusb_device **                           devs,
        bool                                       show_hubs,
        int                                        depth);

#ifdef OLD
bool get_raw_report_descriptor_old(
        struct libusb_device_handle * dh,
        uint8_t                       bInterfaceNumber,
        uint16_t                      rptlen,        // report length
        Byte *                        dbuf,
        int                           dbufsz,
        int *                         pbytes_read);
#endif

bool get_raw_report_descriptor(
        struct libusb_device_handle * dh,
        uint8_t                       bInterfaceNumber,
        uint16_t                      rptlen,        // report length
        Byte *                        dbuf,
        int                           dbufsz,
        int *                         pbytes_read);

bool get_raw_report(
      struct libusb_device_handle * dh,
      uint8_t                       bInterfaceNumber,
      uint8_t                      report_id,
      uint16_t                      rptlen,        // report length
      Byte *                        dbuf,
      int                           dbufsz,
      int *                         pbytes_read);

bool is_hub_descriptor(const struct libusb_device_descriptor * desc);

// really belongs elsewhere

typedef struct __attribute__((__packed__)) hid_class_descriptor {
   uint8_t     bDescriptorType;
   uint16_t    wDescriptorLength;
} HID_Class_Descriptor;

typedef struct __attribute__((__packed__)) hid_descriptor {
   uint8_t      bLength;
   uint8_t      bDescriptorType;
   uint16_t     bcdHID;
   uint8_t      bCountryCode;
   uint8_t      bNumDescriptors;    // number of class descriptors, always at least 1, i.e. Report descriptor
   uint8_t      bClassDescriptorType;    // start of first class descriptor
   uint16_t     wClassDescriptorLength;
} HID_Descriptor;


void report_hid_descriptor(
        libusb_device_handle * dh,
        uint8_t                bInterfaceNumber,
        HID_Descriptor *       desc,
        int                    depth);



#endif /* LIBUSB_REPORTS_H_ */
