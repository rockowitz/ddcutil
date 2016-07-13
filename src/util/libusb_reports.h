/* libusb_reports.h
 *
 * <copyright>
 * Copyright (C) 2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <libusb-1.0/libusb.h>      // need pkgconfig?

#ifndef LIBUSB_UTIL_H_
#define LIBUSB_UTIL_H_


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


// Lookup descriptive names of constants

char * descriptor_title(Byte val);
char * endpoint_direction_title(Byte val);
char * transfer_type_title(Byte val);
char * class_code_title(Byte val);

// Misc utilities

char * lookup_libusb_string(struct libusb_device_handle * dh, int string_id);
wchar_t * lookup_libusb_string_wide(struct libusb_device_handle * dh, int string_id);

// Report functions for libusb data structures

void report_endpoint_descriptor(
        const struct libusb_endpoint_descriptor *epdesc,
        libusb_device_handle * dh,    // may be null
        int depth);
void report_interface_descriptor(
        const struct libusb_interface_descriptor *inter,
        libusb_device_handle * dh,    // may be null
        int depth);
void report_interface(
      const struct libusb_interface * interface,
      libusb_device_handle * dh,    // may be null
      int depth) ;
void report_config_descriptor(
        const struct libusb_config_descriptor *config,
        libusb_device_handle * dh,    // may be null
        int depth);
void report_device_descriptor(
      const struct libusb_device_descriptor * desc,
                              libusb_device_handle * dh,    // may be null
                              int depth);
void report_dev(
      libusb_device * dev,
      libusb_device_handle * dh,    // may be null
      bool show_hubs, int depth);


bool possible_monitor_dev(libusb_device * dev);
bool is_hub_descriptor(const struct libusb_device_descriptor * desc);

// struct possible_monitor_device;

struct possible_monitor_device {
   libusb_device * libusb_device;
   int             bus;
   int             device_address;
   int             alt_setting;
   int             interface;
   ushort          vid;
   ushort          pid;
   char *          manufacturer_name;
   char *          product_name;
   // conversion is annoying, just retrieve both ascii and wchar version of the serial number
   // wchar_t *       serial_number_wide;
   char *          serial_number;  // retrieved as ASCII, note some usages expect wchar
   struct possible_monitor_device * next;
};
typedef struct possible_monitor_device Possible_Monitor_Device;

struct possible_monitor_device * get_possible_monitors();

void report_possible_monitors(struct possible_monitor_device * mondev_head, int depth);

char *make_path(int bus_number, int device_address, int interface_number);

char *make_path_from_libusb_device(libusb_device *dev, int interface_number);

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

void report_hid_descriptor(HID_Descriptor * desc, int depth);

void init_names();

void probe_libusb(bool possible_monitors_only);

#endif /* LIBUSB_UTIL_H_ */
