/* libusb_reports.c
 *
 * Report libusb data structures
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
 *
 * Portions adapted from lsusb.c (command lsusb) by Thomas Sailer and David Brownell
 */

// Adapted from usbplay2 file libusb_util.c

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <linux/hid.h>     // HID_MAX_DESCRIPTOR_SIZE is here on Mint
#include <linux/uhid.h>    // for HID_MAX_DESCRIPTOR_SIZE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
// #include <wchar.h>

#include "util/data_structures.h"
#include "util/string_util.h"
#include "util/report_util.h"
#include "util/device_id_util.h"

#include "usb_util/base_hid_report_descriptor.h"
#include "usb_util/hid_report_descriptor.h"
#include "usb_util/usb_hid_common.h"

#include "usb_util/libusb_reports.h"


//
// Identifier to name tables
//

Value_Name class_id_table[] = {
      {LIBUSB_CLASS_PER_INTERFACE, "LIBUSB_CLASS_PER_INTERFACE"},
      VN(LIBUSB_CLASS_AUDIO),
 //   {0xff,NULL},
      VN_END
};

Value_Name_Title class_code_table[] = {
      /** In the context of a \ref libusb_device_descriptor "device descriptor",
       * LIBUSB_CLASS_PER_INSTANCE indicates that each interface specifies its
       * own class information and all interfaces operate independently.
       */
      VNT( LIBUSB_CLASS_PER_INTERFACE,       "Per interface"),          // 0
      VNT( LIBUSB_CLASS_AUDIO,               "Audio"),                  // 1
      VNT( LIBUSB_CLASS_COMM,                "Communications"),         // 2
      VNT( LIBUSB_CLASS_HID,                 "Human Interface Device"), // 3
      VNT( LIBUSB_CLASS_PHYSICAL,            "Physical device"),        // 5
      VNT( LIBUSB_CLASS_PRINTER,             "Printer"),                // 7
      VNT( LIBUSB_CLASS_IMAGE,               "Image"),                  // 6
      VNT( LIBUSB_CLASS_MASS_STORAGE,        "Mass storage"),           // 8
      VNT( LIBUSB_CLASS_HUB,                 "Hub"),                    // 9
      VNT( LIBUSB_CLASS_DATA,                "Data"),                   // 10
      VNT( LIBUSB_CLASS_SMART_CARD,          "Smart card"),             // 0x0b
      VNT( LIBUSB_CLASS_CONTENT_SECURITY,    "Content security"),       // 0x0d
      VNT( LIBUSB_CLASS_VIDEO,               "Video"),                  // 0x0e
      VNT( LIBUSB_CLASS_PERSONAL_HEALTHCARE, "Personal healthcare"),    // 0x0f
      VNT( LIBUSB_CLASS_DIAGNOSTIC_DEVICE,   "Diagnostic device"),      // 0xdc
      VNT( LIBUSB_CLASS_WIRELESS,            "Wireless"),               // 0xe0
      VNT( LIBUSB_CLASS_APPLICATION,         "Application"),            // 0xfe
      VNT( LIBUSB_CLASS_VENDOR_SPEC,         "Vendor specific"),        // 0xff
      VNT_END
};

Value_Name_Title descriptor_type_table[] = {
      VNT( LIBUSB_DT_DEVICE,                "Device"),            // 0x01
      VNT( LIBUSB_DT_CONFIG,                "Configuration"),     // 0x02
      VNT( LIBUSB_DT_STRING,                "String"),            // 0x03
      VNT( LIBUSB_DT_INTERFACE,             "Interface"),         // 0x04
      VNT( LIBUSB_DT_ENDPOINT,              "Endpoint"),          // 0x05
      VNT( LIBUSB_DT_BOS,                   "BOS" ),              // 0x0f,
      VNT( LIBUSB_DT_DEVICE_CAPABILITY,     "Device Capability"), // 0x10,
      VNT( LIBUSB_DT_HID,                   "HID"),               // 0x21,
      VNT( LIBUSB_DT_REPORT,                "HID report"),        // 0x22,
      VNT( LIBUSB_DT_PHYSICAL,              "Physical"),          // 0x23,
      VNT( LIBUSB_DT_HUB,                   "Hub"),               // 0x29,
      VNT( LIBUSB_DT_SUPERSPEED_HUB,        "SuperSpeed Hub"),    // 0x2a,
      VNT( LIBUSB_DT_SS_ENDPOINT_COMPANION, "SuperSpeed Endpoint Companion"),  // 0x30
      VNT_END
};


#ifdef REF
/** \ingroup desc
 * Endpoint direction. Values for bit 7 of the
 * \ref libusb_endpoint_descriptor::bEndpointAddress "endpoint address" scheme.
 */
enum libusb_endpoint_direction {
   /** In: device-to-host */
   LIBUSB_ENDPOINT_IN = 0x80,

   /** Out: host-to-device */
   LIBUSB_ENDPOINT_OUT = 0x00
};
#endif


Value_Name_Title endpoint_direction_table[] = {
      VNT( LIBUSB_ENDPOINT_IN, "IN"),
      VNT( LIBUSB_ENDPOINT_OUT, "OUT"),
      VNT_END
      };

#ifdef REF
#define LIBUSB_TRANSFER_TYPE_MASK         0x03    /* in bmAttributes */

/** \ingroup desc
 * Endpoint transfer type. Values for bits 0:1 of the
 * \ref libusb_endpoint_descriptor::bmAttributes "endpoint attributes" field.
 */
enum libusb_transfer_type {
   /** Control endpoint */
   LIBUSB_TRANSFER_TYPE_CONTROL = 0,

   /** Isochronous endpoint */
   LIBUSB_TRANSFER_TYPE_ISOCHRONOUS = 1,

   /** Bulk endpoint */
   LIBUSB_TRANSFER_TYPE_BULK = 2,

   /** Interrupt endpoint */
   LIBUSB_TRANSFER_TYPE_INTERRUPT = 3,

   /** Stream endpoint */
   LIBUSB_TRANSFER_TYPE_BULK_STREAM = 4,
};

#endif

// Problem: LIBUSB_TRANFER_TYPE_BULK_STREAM not defined in 1.0.17
#define LIBUSB_TRANSFER_TYPE_BULK_STREAM_LOCAL 4


Value_Name_Title transfer_type_table[] = {
      VNT(LIBUSB_TRANSFER_TYPE_CONTROL,     "Control"),          // 0
      VNT(LIBUSB_TRANSFER_TYPE_ISOCHRONOUS, "Isochronous"),      // 1
      VNT(LIBUSB_TRANSFER_TYPE_BULK,        "Bulk"),             // 2
      VNT(LIBUSB_TRANSFER_TYPE_INTERRUPT,   "Interrupt"),        // 3
      VNT(LIBUSB_TRANSFER_TYPE_BULK_STREAM_LOCAL, "Bulk Stream"),      // 4
      VNT_END
};





char * descriptor_title(Byte val) {
   return vnt_title(descriptor_type_table, val);
}

char * endpoint_direction_title(Byte val) {
   return vnt_title(endpoint_direction_table, val);
}

char * transfer_type_title(Byte val) {
   return vnt_title(transfer_type_table, val);
}

char * class_code_title(Byte val) {
   return vnt_title(class_code_table, val);
}



//
// Misc Utilities
//

#define LIBUSB_STRING_BUFFER_SIZE 100
char libusb_string_buffer[LIBUSB_STRING_BUFFER_SIZE];

char * lookup_libusb_string(struct libusb_device_handle * dh, int string_id) {
   int rc = libusb_get_string_descriptor_ascii(
               dh,
               string_id,
               (unsigned char *) libusb_string_buffer,
               LIBUSB_STRING_BUFFER_SIZE);
   if (rc < 0) {
      REPORT_LIBUSB_ERROR("libusb_get_string_descriptor_ascii",  rc, LIBUSB_CONTINUE);
      strcpy(libusb_string_buffer, "<Unknown string>");
   }
   else {
      // 7/2017: assert was triggered by user, why?
      // replace assert by diagnostic message
      printf("(%s) rc=%d, LIBUSB_STRING_BUFFER_SIZE=%d, strlen=%zu\n",
            __func__, rc, LIBUSB_STRING_BUFFER_SIZE,    strlen(libusb_string_buffer) );
      // assert(rc == strlen(libusb_string_buffer));
   }
   return libusb_string_buffer;
}

#ifdef UNUSED
// unused, and requires include of wchar.h
wchar_t libusb_string_buffer_wide[LIBUSB_STRING_BUFFER_SIZE];

wchar_t * lookup_libusb_string_wide(struct libusb_device_handle * dh, int string_id) {
   int rc = libusb_get_string_descriptor(
               dh,
               string_id,
               33,         // US English
               (unsigned char *) libusb_string_buffer_wide,  // N. CAST
               LIBUSB_STRING_BUFFER_SIZE);
   if (rc < 0) {
        REPORT_LIBUSB_ERROR("libusb_get_string_descriptor_wide",  rc, LIBUSB_CONTINUE);
        wcscpy(libusb_string_buffer_wide, L"<Unknown string>");
   }
   else {
      // printf("(%s) rc=%d, wcslen(libusb_string_buffer_wide)=%d, strlen=%d\n",
      //       __func__, rc, wcslen(libusb_string_buffer_wide), strlen(libusb_string_buffer_wide) );
      // assert(rc == wcslen(libusb_string_buffer));

   }
   return (wchar_t *) libusb_string_buffer;
}
#endif


// from lsusb.c
/* ---------------------------------------------------------------------- */

/* workaround libusb API goofs:  "byte" should never be sign extended;
 * using "char" is trouble.  Likewise, sizes should never be negative.
 */

static inline int
typesafe_control_msg(
      libusb_device_handle * dev,
      unsigned char          requesttype,
      unsigned char          request,
      int                    value,
      int                    idx,
      unsigned char *        bytes,
      unsigned               size,
      int                    timeout)
{
   int ret = libusb_control_transfer(
                dev, requesttype, request, value, idx, bytes, size, timeout);

   if (ret < 0)
      return -ret;
   else
      return ret;
}

#define usb_control_msg    typesafe_control_msg


//
// Report functions for libusb data structures
//

void report_libusb_endpoint_descriptor(
        const struct libusb_endpoint_descriptor * epdesc,
        libusb_device_handle *                    dh,    // may be null
        int                                       depth)
{
   int d1 = depth+1;
   rpt_structure_loc("libusb_endpoint_descriptor", epdesc, depth);

   rpt_vstring(d1, "%-20s 0x%02x  %s",
                   "bDescriptorType:",
                   epdesc->bDescriptorType,
                   descriptor_title(epdesc->bDescriptorType)
              );

   /** bEndpointAddress: The address of the endpoint described by this descriptor. Bits 0:3 are
    * the endpoint number. Bits 4:6 are reserved. Bit 7 indicates direction,
    * see \ref libusb_endpoint_direction.
    */
   unsigned char endpoint_number = epdesc->bEndpointAddress & 0x0f;
   char * direction_name = (epdesc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ? "IN" : "OUT";
   rpt_vstring(d1, "%-20s 0x%02x  Endpoint number: %d  Direction: %s",
               "bEndpointAddress:",
               epdesc->bEndpointAddress,
               endpoint_number,
               direction_name);

   /** Attributes which apply to the endpoint when it is configured using
    * the bConfigurationValue. Bits 0:1 determine the transfer type and
    * correspond to \ref libusb_transfer_type. Bits 2:3 are only used for
    * isochronous endpoints and correspond to \ref libusb_iso_sync_type.
    * Bits 4:5 are also only used for isochronous endpoints and correspond to
    * \ref libusb_iso_usage_type. Bits 6:7 are reserved.
    */
   // uint8_t  bmAttributes;
   Byte transfer_type = epdesc->bmAttributes & 0x03;
   // Byte isoc_sync_type     = epdesc->bmAttributes & 0x0c;     // unused
   // Byte isoc_iso_usage_type = epdesc->bmAttributes & 0x30;    // unused
   rpt_vstring(d1, "%-20s 0x%02x  Transfer Type: %s",
                   "bmAttributes:",
                   epdesc->bmAttributes,
                   transfer_type_title(transfer_type)
                   );

   /** Maximum packet size this endpoint is capable of sending/receiving. */
   // uint16_t wMaxPacketSize;
   rpt_vstring(d1, "%-20s %u",
                   "wMaxPacketSize:",
                   epdesc->wMaxPacketSize);

   /** Interval for polling endpoint for data transfers. */
   // uint8_t  bInterval;
   rpt_vstring(d1, "%-20s %d     %s",
                   "bInterval",
                   epdesc->bInterval,
                   "(data transfer polling interval)"
                  );

   // skipping several

   /** Length of the extra descriptors, in bytes. */
   // int extra_length;
   rpt_vstring(d1, "%-20s %d     (length of extra descriptors)",
                   "extra_length:",
                   epdesc->extra_length);
}


// from lsusb.c



/* Read a control message
 *
 * Arguments:
 *   dh              libusb display handle
 *   bmRequestType
 *   bRequest
 *   wValue
 *   wIndex          bInterfaceNumber parm
 *   dbuf            pointer to buffer
 *   dbufsz          buffer size
 *   WLength         number of bytes to try to read
 *   pbytes_read     return number of bytes actually read here
 *
 * Returns:          true if success, false if not
 */
bool call_read_control_msg(
        struct libusb_device_handle * dh,
        uint8_t                       bmRequestType,
        uint8_t                       bRequest,
        uint16_t                      wValue,
        uint16_t                      wIndex,  // = bInterfaceNumber
        Byte *                        dbuf,     // data
        uint16_t                      dbufsz,
        uint16_t                      wLength,       // report length
        int *                         pbytes_read)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting\n", __func__);

   assert(dh);
   assert( dbufsz >= wLength);

   // const int CTRL_RETRIES = 2;
   const int CTRL_TIMEOUT = (5*1000); /* milliseconds */

   bool ok = false;
   int bytes_read = 0;

   uint16_t bInterfaceNumber = wIndex;
   uint16_t rptlen = wLength;

   int rc = libusb_claim_interface(dh, bInterfaceNumber);
   if (rc != 0) {
      // TODO: replace w proper diagnostic message using libusb functions
      printf("(%s) libusb_claim_inteface returned %d\n", __func__, rc);
      goto bye;
   }

   int retries = 4;
   while (bytes_read < rptlen && retries--) {
      bytes_read = usb_control_msg(
                      dh,
                      bmRequestType,
                      bRequest,
                      wValue,
                      bInterfaceNumber,   // = wIndex
                      dbuf,
                      rptlen,             // = wLength
                      CTRL_TIMEOUT);
   }
   if (bytes_read > 0) {
      ok = true;
   }
   libusb_release_interface(dh, bInterfaceNumber);

bye:
   *pbytes_read = bytes_read;
   if (debug)
      printf("(%s) Returning: %s, *pbytes_read=%d\n", __func__, bool_repr(ok), *pbytes_read);
   return ok;
}


/* Get bytes of HID Report Descriptor
 *
 * Arguments:
 *   dh
 *   bInterfaceNumber
 *   rptlen
 *   dbuf
 *   dbufsz
 *
 * Returns:        true if success, false if not
 */
bool get_raw_report_descriptor(
        struct libusb_device_handle * dh,
        uint8_t                       bInterfaceNumber,
        uint16_t                      rptlen,        // report length
        Byte *                        dbuf,
        int                           dbufsz,
        int *                         pbytes_read)
{
   bool ok = false;
   assert(dh);

   ok = call_read_control_msg(
         dh,                                                                               // dev_handle
         LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_INTERFACE,   // bmRequestType
         LIBUSB_REQUEST_GET_DESCRIPTOR,     // bRequest
         LIBUSB_DT_REPORT << 8 | 0x0,       // wValue - the value field for the setup packet
         bInterfaceNumber,                  // ?? wIndex - index field for the setup packet  ???
         dbuf,                              // data
         dbufsz,
         rptlen,                            // wLength
         pbytes_read
         );

   if (ok && *pbytes_read < rptlen) {
      printf("          Warning: incomplete report descriptor\n");
      // dump_report_desc(dbuf, *pbytes_read);    // old way
   }

   return ok;
}


// TODO: move to more appropriate location
// HID Class-Specific Requests values. See section 7.2 of the HID specifications
#define HID_GET_REPORT                0x01
#define HID_GET_IDLE                  0x02
#define HID_GET_PROTOCOL              0x03
#define HID_SET_REPORT                0x09
#define HID_SET_IDLE                  0x0A
#define HID_SET_PROTOCOL              0x0B
#ifdef ALREADY_DEFINED
// defined in hid_report_descriptor.h
#define HID_REPORT_TYPE_INPUT         0x01
#define HID_REPORT_TYPE_OUTPUT        0x02
#define HID_REPORT_TYPE_FEATURE       0x03
#endif



/* Get bytes of HID Feature Report
 *
 * Arguments:
 *   dh                 libusb device handle
 *   bInterfaceNumber   interface number
 *   report_id          feature report number
 *   rptlen             bytes requested (may be larger than actual report size)
 *   dbuf               pointer to buffer
 *   dbufsz             buffer size
 *   pbytes_read        return number of bytes read here
 *
 * Returns:        true if success, false if not
 */
bool get_raw_report(
      struct libusb_device_handle * dh,
      uint8_t                       bInterfaceNumber,
      uint8_t                       report_id,
      uint16_t                      rptlen,        // report length
      Byte *                        dbuf,
      int                           dbufsz,
      int *                         pbytes_read)
{
   bool ok = false;

   ok = call_read_control_msg(
         dh,                                                                               // dev_handle
         LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,   // bmRequestType
         HID_GET_REPORT,             // bRequest
         HID_REPORT_TYPE_FEATURE << 8 | report_id,              // wValue - the value field for the setup packet
         bInterfaceNumber,                  // ?? wIndex - index field for the setup packet  ???
         dbuf,                              // data
         dbufsz,
         rptlen,                            // wLength
         pbytes_read
         );

   return ok;
}


 /* Reports struct libusb_interface_descriptor
  *
  * Arguments:
  *   inter      pointer to libusb_interface_descriptor instance
  *   dh         display handle, not required but allows for additional information
  *   depth      logical indentation depth
  *
  * Returns:     nothing
  */
void report_libusb_interface_descriptor(
        const struct libusb_interface_descriptor * inter,
        libusb_device_handle *                     dh,    // may be null
        int                                        depth)
{
   int d1 = depth+1;

   rpt_structure_loc("libusb_interface_descriptor", inter, depth);

   /** Size of this descriptor (in bytes) */
   // uint8_t  bLength;
   rpt_vstring(d1, "%-20s %d", "bLength", inter->bLength);

   /** Descriptor type. Will have value
    * \ref libusb_descriptor_type::LIBUSB_DT_INTERFACE LIBUSB_DT_INTERFACE
    * in this context. */
   // uint8_t  bDescriptorType;
   rpt_vstring(d1, "%-20s 0x%02x  %s",
                   "bDescriptorType:",
                   inter->bDescriptorType,
                   descriptor_title(inter->bDescriptorType)
              );

   /** Number of this interface */
   // uint8_t  bInterfaceNumber;
   rpt_vstring(d1, "%-20s %u",
                   "bInterfaceNumber:",
                   inter->bInterfaceNumber);


   /** Value used to select this alternate setting for this interface */
   // uint8_t  bAlternateSetting;
   rpt_vstring(d1, "%-20s %u", "bAlternateSetting:", inter->bAlternateSetting);


   /** Number of endpoints used by this interface (excluding the control
    * endpoint). */
   // uint8_t  bNumEndpoints;
   rpt_vstring(d1, "%-20s %u", "bNumEndpoints:", inter->bNumEndpoints);

   /** USB-IF class code for this interface. See \ref libusb_class_code. */
   // uint8_t  bInterfaceClass;
   rpt_vstring(d1, "%-20s %u  (0x%02x)  %s",
                   "bInterfaceClass:",
                   inter->bInterfaceClass,
                   inter->bInterfaceClass,
                   class_code_title(inter->bInterfaceClass) );

   /** USB-IF subclass code for this interface, qualified by the
    * bInterfaceClass value */
   // uint8_t  bInterfaceSubClass;
   rpt_vstring(d1, "%-20s %u  (0x%02x)  %s",
                   "bInterfaceSubClass:",
                   inter->bInterfaceSubClass,
                   inter->bInterfaceSubClass,
                   "");

   /** USB-IF protocol code for this interface, qualified by the
    * bInterfaceClass and bInterfaceSubClass values */
   // uint8_t  bInterfaceProtocol;
   rpt_vstring(d1, "%-20s %u  (0x%02x)  %s",
                   "bInterfaceProtocol:",
                   inter->bInterfaceProtocol,
                   inter->bInterfaceProtocol,
                   "");

   // Index of string descriptor describing this interface: uint8_t  iInterface;
   char * interface_name = "";
   if (dh && inter->iInterface > 0)
      interface_name = lookup_libusb_string(dh, inter->iInterface);
   rpt_vstring(d1, "%-20s %d  \"%s\" ",
                   "iInterface",
                   inter->iInterface,
                   interface_name
                   );


   /** Array of endpoint descriptors. This length of this array is determined
    * by the bNumEndpoints field. */
   // const struct libusb_endpoint_descriptor *endpoint;
   // NB: This is an array of endpoint descriptors,
   //     not an array of pointers to endpoint descriptors
   //     nor a pointer to array of pointers to endpoint descriptors
   int ndx = 0;
   for (ndx=0; ndx<inter->bNumEndpoints; ndx++) {
      const struct libusb_endpoint_descriptor *epdesc = &(inter->endpoint[ndx]);
      report_libusb_endpoint_descriptor(epdesc, dh, d1);
   }

   /** Extra descriptors. If libusb encounters unknown interface descriptors,
    * it will store them here, should you wish to parse them. */
   // const unsigned char *extra;

   /** Length of the extra descriptors, in bytes. */
   // int extra_length;
   rpt_vstring(d1, "%-20s %d     (length of extra descriptors)",
                   "extra_length:",
                   inter->extra_length);
   if (inter->extra_length > 0) {
      rpt_vstring(d1, "extra at %p: ", inter->extra);
      rpt_hex_dump(inter->extra, inter->extra_length, d1);

      if (dh) {
         if (inter->bInterfaceClass == LIBUSB_CLASS_HID) {  // 3
            const Byte * cur_extra = inter->extra;
            int remaining_length = inter->extra_length;
            while (remaining_length > 0) {
               HID_Descriptor * cur_hid_desc = (HID_Descriptor *) cur_extra;
               assert(cur_hid_desc->bLength <= remaining_length);
               report_hid_descriptor(dh, inter->bInterfaceNumber, cur_hid_desc, d1);

               cur_extra += cur_hid_desc->bLength;
               remaining_length -= cur_hid_desc->bLength;
            }
         }
      }
   }
}


/* Reports struct libusb_interface
 *
 * Arguments:
 *   inter      pointer to libusb_interface instance
 *   dh         display handle, not required but allows for additional information
 *   depth      logical indentation depth
 *
 * Returns:     nothing
 *
 * struct libusb_interface represents a collection of alternate settings for a
 * particular USB interface.  It contains an array of interface descriptors, one
 * for each alternate settings.
 */
void report_libusb_interface(
      const struct libusb_interface *  interface,
      libusb_device_handle *           dh,    // may be null
      int                              depth)
{
   int d1 = depth+1;
   rpt_structure_loc("libusb_interface", interface, depth);

   // The number of alternate settings that belong to this interface
   rpt_vstring(d1, "%-20s  %d  (number of alternate settings for this interface)",
                   "num_altsetting", interface->num_altsetting);
   // rpt_int("num_altsetting", NULL, interface->num_altsetting, d1);

   for (int ndx=0; ndx<interface->num_altsetting; ndx++) {
      report_libusb_interface_descriptor(&interface->altsetting[ndx], dh, d1);
   }
}


/* Reports struct libusb_config_descriptor
 *
 * Arguments:
 *   config     pointer to libusb_config_descriptor instance
 *   dh         display handle, not required but allows for additional information
 *   depth      logical indentation depth
 *
 * Returns:     nothing
 *
 * struct libusb_config_descriptor represents the standard USB configuration
 * descriptor. This descriptor is documented in section 9.6.3 of the USB 3.0
 * specification.  It contains multiple libusb_interface structs.
 *
 * All multiple-byte fields are represented in host-endian format.
 */
void report_libusb_config_descriptor(
        const struct libusb_config_descriptor * config,
        libusb_device_handle *                  dh,    // may be null
        int                                     depth)
{
   int d1 = depth+1;

   rpt_structure_loc("libusb_config_descriptor", config, depth);

   // Size of this descriptor (in bytes): uint8_t  bLength;
   rpt_vstring(d1, "%-20s  %d", "bLength:", config->bLength, d1);


   // Descriptor type. Will have value LIBUSB_DT_CONFIG in this context.
   rpt_vstring(d1, "%-20s 0x%02x  %s",
                   "bDescriptorType:",
                   config->bDescriptorType,              // uint8_t  bDescriptorType;
                   descriptor_title(config->bDescriptorType)
              );

   /** Total length of data returned for this configuration */
   //uint16_t wTotalLength;

   /** Number of interfaces supported by this configuration */
   // uint8_t  bNumInterfaces;
   rpt_int("bNumInterfaces", NULL, config->bNumInterfaces, d1);

   /** Identifier value for this configuration */
   // uint8_t  bConfigurationValue;
   rpt_int("bConfigurationValue", "id for this configuration", config->bConfigurationValue, d1);

   /** Index of string descriptor describing this configuration */
   // uint8_t  iConfiguration;
   rpt_int("iConfiguration", "index of string descriptor", config->iConfiguration, d1);

   /** Configuration characteristics */
   // uint8_t  bmAttributes;
   rpt_uint8_as_hex("bmAttributes", "config characteristics", config->bmAttributes, d1);

   /** Maximum power consumption of the USB device from this bus in this
    * configuration when the device is fully opreation. Expressed in units
    * of 2 mA. */
   // uint8_t  MaxPower;
   rpt_int("MaxPower", "units of 2 mA", config->MaxPower, d1);

   /** Array of interfaces supported by this configuration. The length of
    * this array is determined by the bNumInterfaces field. */
   // const struct libusb_interface *interface;
   int ndx = 0;
   for (ndx=0; ndx<config->bNumInterfaces; ndx++) {
      const struct libusb_interface *inter = &(config->interface[ndx]);
      report_libusb_interface(inter, dh, d1);
   }


   /** Extra descriptors. If libusb encounters unknown configuration
    * descriptors, it will store them here, should you wish to parse them. */
   // const unsigned char *extra;

   /** Length of the extra descriptors, in bytes. */
   // int extra_length;
   rpt_int("extra_length", "len of extra descriptors", config->extra_length, d1);

}


/* Reports struct libusb_device_descriptor.
 *
 * Arguments:
 *    desc             pointer to libusb_device_descriptor instance
 *    dh               if non-null, string values are looked up for string descriptor indexes
 *    depth            logical indentation depth
 *
 * Returns:    nothing
 *
 * struct libusb_device_descriptor represents the standard USB device descriptor.
 * This descriptor is documented in section 9.6.1 of the USB 3.0 specification.
 *
 * All multiple-byte fields are represented in host-endian format.
 */
void report_libusb_device_descriptor(
        const struct libusb_device_descriptor * desc,
        libusb_device_handle *                  dh,    // may be null
        int                                     depth)
{
   int d1 = depth+1;

   rpt_structure_loc("libusb_device_descriptor", desc, depth);

   // Size of this descriptor (in bytes):  uint8_t  bLength;
   rpt_vstring(d1, "%-20s %d", "bLength:", desc->bLength);

   // Descriptor type. Will have value LIBUSB_DT_DEVICE in this context.
   rpt_vstring(d1, "%-20s 0x%02x  %s",
                   "bDescriptorType:",
                   desc->bDescriptorType,          // uint8_t  bDescriptorType;
                   descriptor_title(desc->bDescriptorType) );

   /** USB specification release number in binary-coded decimal. A value of
    * 0x0200 indicates USB 2.0, 0x0110 indicates USB 1.1, etc. */
   // uint16_t bcdUSB;
   unsigned int bcdHi  = desc->bcdUSB >> 8;
   unsigned int bcdLo  = desc->bcdUSB & 0x0f;
   rpt_vstring(d1,"%-20s 0x%04x (%x.%02x)",
                  "bcdUSB",
                  desc->bcdUSB,
                  bcdHi,
                  bcdLo);

   /** USB-IF class code for the device. See \ref libusb_class_code. */
   rpt_vstring(d1, "%-20s 0x%02x  (%u)  %s",
                   "bDeviceClass:",
                   desc->bDeviceClass,             // uint8_t  bDeviceClass;
                   desc->bDeviceClass,
                   class_code_title(desc->bDeviceClass) );

   /** USB-IF subclass code for the device, qualified by the bDeviceClass value */
   // uint8_t  bDeviceSubClass;
   rpt_vstring(d1, "%-20s 0x%02x (%u)", "bDeviceSubClass:",
                   desc->bDeviceSubClass, desc->bDeviceSubClass);

   /** USB-IF protocol code for the device, qualified by the bDeviceClass and
    * bDeviceSubClass values */
   // uint8_t  bDeviceProtocol;
   // rpt_int("bDeviceProtocol", NULL, desc->bDeviceProtocol, d1);
   rpt_vstring(d1, "%-20s 0x%02x (%u)", "bDeviceProtocol:", desc->bDeviceProtocol, desc->bDeviceProtocol);

   /** Maximum packet size for endpoint 0 */
   // uint8_t  bMaxPacketSize0;
   rpt_vstring(d1, "%-20s %u  (max size for endpoint 0)", "bMaxPacketSize0:", desc->bMaxPacketSize0);

   Pci_Usb_Id_Names usb_id_names =
            devid_get_usb_names(
                      desc->idVendor,
                      desc->idProduct,
                      0,
                      2);

   // USB-IF vendor ID:  uint16_t idVendor;
   rpt_vstring(d1, "%-20s 0x%04x  %s", "idVendor:", desc->idVendor, usb_id_names.vendor_name);

   // USB-IF product ID:  uint16_t idProduct;
   rpt_vstring(d1, "%-20s 0x%04x  %s", "idProduct:", desc->idProduct, usb_id_names.device_name);

   // Device release number in binary-coded decimal: uint16_t bcdDevice;
   bcdHi  = desc->bcdDevice >> 8;
   bcdLo  = desc->bcdDevice & 0x0f;
   rpt_vstring(d1, "%-20s %2x.%02x  (device release number)", "bcdDevice:", bcdHi, bcdLo);


   // Index of string descriptor describing manufacturer: uint8_t  iManufacturer;
   // rpt_vstring(d1, "%-20s %u  (mfg string descriptor index)", "iManufacturer:", desc->iManufacturer);

   char *    mfg_name = "";
   // wchar_t * mfg_name_w = L"";

   if (dh && desc->iManufacturer) {
      mfg_name = lookup_libusb_string(dh, desc->iManufacturer);
      // mfg_name_w =  lookup_libusb_string_wide(dh, desc->iManufacturer) ;
      // wprintf(L"Manufacturer (wide) %d -%ls\n",
      //          desc->iManufacturer,
      //            lookup_libusb_string_wide(dh, desc->iManufacturer) );
   }
   rpt_vstring(d1, "%-20s %d  %s", "iManufacturer:", desc->iManufacturer, mfg_name);
   // rpt_vstring(d1, "%-20s %u  %S", "iManufacturer:", desc->iManufacturer, mfg_name_w);


   // Index of string descriptor describing product: uint8_t  iProduct;
   // rpt_int("iProduct", "product string descriptor index", desc->iProduct, d1);

   char *    product_name = "";
   if (dh && desc->iProduct)
      product_name = lookup_libusb_string(dh, desc->iProduct);
   rpt_vstring(d1, "%-20s %u  %s", "iProduct:", desc->iProduct, product_name);


   //Index of string descriptor containing device serial number: uint8_t  iSerialNumber;
   // rpt_int("iSerialNumber", "index of string desc for serial num", desc->iProduct, d1);

   char *    sn_name = "";
   if (dh && desc->iSerialNumber)
      sn_name = lookup_libusb_string(dh, desc->iSerialNumber);
   rpt_vstring(d1, "%-20s %u  %s", "iSerialNumber:", desc->iSerialNumber, sn_name);



   // Number of possible configurations:  uint8_t  bNumConfigurations;
   rpt_vstring(d1, "%-20s %u (number of possible configurations)", "bNumConfigurations:", desc->bNumConfigurations);
}


char * format_port_number_path(unsigned char path[], int portct, char * buf, int bufsz) {
   *buf = 0;
   int ndx;

   for (ndx=0; ndx < portct; ndx++) {
      char *end = buf + strlen(buf);
      // printf("end=%p\n", end);
      if (ndx == 0)
         sprintf(end, "%u", path[ndx]);
      else
         sprintf(end, ".%u", path[ndx]);
    }
    return buf;
}


bool is_hub_descriptor(const struct libusb_device_descriptor * desc) {
   return (desc->bDeviceClass == 9);
}

#ifdef IN_PROGRESS
void report_open_dev(
      libusb_device *         dev,
      libusb_device_handle *  dh,    // must not be null
      bool                    show_hubs,
      int                     depth)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting.  dev=%p, dh=%p, show_hubs=%s\n", __func__, dev, dh, bool_repr(show_hubs));

   assert(dev);
   assert(dh);

   int d1 = depth+1;
   int rc;



}
#endif



/* Reports a single libusb device.
 *
 */
void report_libusb_device(
      libusb_device *         dev,
      bool                    show_hubs,
      int                     depth)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. dev=%p, show_hubs=%s\n", __func__, dev, bool_repr(show_hubs));

   int d1 = depth+1;
   int rc;
   // int j;

   // if (debug) {
   rpt_structure_loc("libusb_device", dev, depth);
   uint8_t busno = libusb_get_bus_number(dev);
   uint8_t devno = libusb_get_device_address(dev);

   rpt_vstring(d1, "%-20s: %d  (0x%04x)", "Bus number",     busno, busno);
   rpt_vstring(d1, "%-20s: %d  (0x%04x)", "Device address", devno, devno);
   // }
   // else {
   //    rpt_vstring(depth, "USB bus:device = %d:%d", libusb_get_bus_number(dev), libusb_get_device_address(dev));
   // }
   uint8_t portno = libusb_get_port_number(dev);
   rpt_vstring(d1, "%-20s: %u (%s)", "Port number",
                   portno,
                   "libusb_get_port_number(), number of the port this device is connected to");

   /* uint8_t */ unsigned char path[8];
   int portct = libusb_get_port_numbers(dev, path, sizeof(path));
   char buf[100];
   format_port_number_path(path, portct, buf, 100);
   rpt_vstring(d1, "%-20s: %s (list of all port numbers from root)", "Port numbers", buf);

   struct libusb_device_descriptor desc;
   // copies data into struct pointed to by desc, does not allocate:
   rc = libusb_get_device_descriptor(dev, &desc);
   CHECK_LIBUSB_RC("libusb_get_device_descriptor", rc, LIBUSB_EXIT);

   if ( !show_hubs && is_hub_descriptor(&desc)) {
      rpt_title("Is hub device, skipping detail", d1);
   }
   else {
      struct libusb_device_handle * dh = NULL;
      int rc = libusb_open(dev, &dh);
      if (rc < 0) {
         REPORT_LIBUSB_ERROR("libusb_open", rc, LIBUSB_CONTINUE);
         dh = NULL;   // belt and suspenders
      }
      else {
         if (debug)
            printf("(%s) Successfully opened\n", __func__);
         int has_detach_kernel_capability =
               libusb_has_capability(LIBUSB_CAP_SUPPORTS_DETACH_KERNEL_DRIVER);
         if (debug)
            printf("(%s) %s kernel detach driver capability\n",
                   __func__,
                   (has_detach_kernel_capability) ? "Has" : "Does not have");

         if (has_detach_kernel_capability) {
            rc = libusb_set_auto_detach_kernel_driver(dh, 1);
            if (rc < 0) {
               REPORT_LIBUSB_ERROR("libusb_set_auto_detach_kernel_driver", rc, LIBUSB_CONTINUE);
            }
         }

      }

#ifdef TEMPORARY_TEST
      if (dh) {
      // printf("String 0:\n");
      // printf("%s\n", lookup_libusb_string(dh, 0));    // INVALID_PARM
      printf("String 1:\n");
      printf("%s\n", lookup_libusb_string(dh, 1));
      }
#endif

      report_libusb_device_descriptor(&desc, dh, d1);

      struct libusb_config_descriptor *config;
      libusb_get_config_descriptor(dev, 0 /* config_index */, &config);  // returns a pointer
      report_libusb_config_descriptor(config, dh, d1);
      libusb_free_config_descriptor(config);

      if (dh)
         libusb_close(dh);
   }
   printf("\n");
   if (debug)
      printf("(%s) Done\n", __func__);
}


// Report a list of libusb_devices
void report_libusb_devices(libusb_device **devs, bool show_hubs, int depth)
{
      libusb_device *dev;

      int i = 0;
      while ((dev = devs[i++]) != NULL) {
         puts("");
         report_libusb_device(dev,  show_hubs, depth);
      }
}



#ifdef REF
typedef struct hid_class_descriptor {
   uint8_t     bDescriptorType;
   uint16_t    wDescriptorLength;
} HID_Class_Descriptor;

typedef struct hid_descriptor {
   uint8_t      bLength;
   uint8_t      bDescriptorType;
   uint16_t     bcdHID;
   uint8_t      bCountryCode;
   uint8_t      bNumDescriptors;    // number of class descriptors, always at least 1, i.e. Report descriptor
   uint8_t      bDescriptorType;    // start of first class descriptor
   uint16_t     wDescriptorLength;
} HID_Descriptor;
#endif



static void report_retrieved_report_descriptor_and_probe(
               libusb_device_handle* dh,
               Byte *                dbuf,
               int                   dbufct,
               int                   depth)
{
   int d1 = depth+1;
   int d2 = depth+2;
   // int d3 = depth+3;

   Byte buf[1024] = {0};
   int bytes_read = 0;

   rpt_vstring(depth, "Displaying report descriptor in HID external form:");
   Hid_Report_Descriptor_Item* item_list = tokenize_hid_report_descriptor(dbuf, dbufct);
   report_hid_report_item_list(item_list, d1);
   puts("");
   Parsed_Hid_Descriptor* phd = parse_hid_report_desc_from_item_list(item_list);
   if (phd) {
      rpt_vstring(depth, "Parsed report descriptor:");
      report_parsed_hid_descriptor(phd, d1);
      puts("");

      rpt_vstring(d1, "Finding HID report for EDID...");
      Parsed_Hid_Report* edid_report_desc = find_edid_report_descriptor(phd);
      if (!edid_report_desc) {
         rpt_vstring(d2, "Not found");
      } else {
         // get EDID report
         report_parsed_hid_report(edid_report_desc, d1);
         rpt_vstring(d1, "Get report data for EDID");
         uint16_t rptlen = 258;

         bytes_read = 0;
         uint16_t report_id = edid_report_desc->report_id;
         bool ok = get_raw_report(
               dh,
               0,                      // interface number  TODO
               report_id,
               rptlen,
               buf,
               1024,
               &bytes_read);
         if (!ok)
            printf("(%s) Error reading report\n", __func__);
         else {
            rpt_vstring(d2, "Read %d bytes for report %d 0x%02x for EDID", bytes_read,
                  report_id, report_id);
            rpt_hex_dump(buf, bytes_read, d2);
         }
      }

      // VCP Codes
      puts("");
      rpt_vstring(d1, "Finding HID feature reports for VCP features...");
      GPtrArray* vcp_code_report_descriptors = get_vcp_code_reports(phd);
      if (vcp_code_report_descriptors && vcp_code_report_descriptors->len > 0) {
         for (int ndx = 0; ndx < vcp_code_report_descriptors->len; ndx++) {
            Vcp_Code_Report* vcr = g_ptr_array_index(vcp_code_report_descriptors, ndx);
            // puts("");
            summarize_vcp_code_report(vcr, d2);
            rpt_vstring(d2, "Get report data for VCP feature 0x%02x", vcr->vcp_code);
            uint16_t rptlen = 3;
            bytes_read = 0;
            uint16_t report_id = vcr->rpt->report_id;
            bool ok = get_raw_report(
                         dh,
                         0, // interface number  TODO
                         report_id,
                         rptlen,
                         buf,
                         1024,
                         &bytes_read);
            if (!ok)
               printf("(%s) Error reading report\n", __func__);
            else {
               rpt_vstring(d2, "Read %d bytes for report %d 0x%02x for vcp feature 0x%02x",
                     bytes_read, report_id, report_id, vcr->vcp_code);
               rpt_hex_dump(buf, bytes_read, d2);
            }
            puts("");
         }
      } else {
         rpt_vstring(d2, "Not found");
         puts("");
      }
      free_parsed_hid_descriptor(phd);
   }
   free_hid_report_item_list(item_list);
}


/* Reports a HID_Descriptor
 *
 * Arguments:
 *    dh                libusb device handle
 *    bInterfaceNumber  interface number
 *    desc              pointer to HID_Descriptor to report
 *    depth             logical indentation depth
 *
 * Returns:
 *    nothing
 */
void report_hid_descriptor(
        libusb_device_handle * dh,
        uint8_t                bInterfaceNumber,
        HID_Descriptor *       desc,
        int                    depth)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. dh=%p, bInterfaceNumber=%d, desc=%p\n",
            __func__, dh, bInterfaceNumber, desc);
   int d1 = depth+1;

   rpt_structure_loc("HID_Descriptor", desc, depth);

   rpt_vstring(d1, "%-20s   %u", "bLength", desc->bLength);
   rpt_vstring(d1, "%-20s   %u  %s", "bDescriptorType", desc->bDescriptorType,  descriptor_title(desc->bDescriptorType));
   rpt_vstring(d1, "%-20s   %2x.%02x  (0x%04x)", "bcdHID",
                   desc->bcdHID>>8, desc->bcdHID & 0x0f, desc->bcdHID);
   rpt_vstring(d1, "%-20s   %u", "bCountryCode", desc->bCountryCode);
   rpt_vstring(d1, "%-20s   %u", "bNumDescriptors", desc->bNumDescriptors);

   rpt_vstring(d1, "first bDescriptorType is at %p", &desc->bClassDescriptorType);
   int ndx = 0;
   for (;ndx < desc->bNumDescriptors; ndx++) {
      assert(sizeof(HID_Class_Descriptor) == 3);
      int offset = ndx * sizeof(HID_Class_Descriptor);
      HID_Class_Descriptor * cur =  (HID_Class_Descriptor *) (&desc->bClassDescriptorType + offset);
      rpt_vstring(d1, "cur = %p", cur);
      rpt_vstring(d1, "%-20s   %u  %s", "bDescriptorType",
                      cur->bDescriptorType, descriptor_title(cur->bDescriptorType));
      uint16_t descriptor_len = cur->wDescriptorLength;    // assumes we're on little endian system
      // uint16_t rpt_len = buf[7+3*i] | (buf[8+3*i] <<
      rpt_vstring(d1, "%-20s   %u", "wDescriptorLength", descriptor_len);

      switch(cur->bDescriptorType) {
      case LIBUSB_DT_REPORT:
      {
         rpt_vstring(d1, "Reading report descriptor of type LIBUSB_DT_REPORT from device...");

         Byte dbuf[HID_MAX_DESCRIPTOR_SIZE];

         if (dh == NULL) {
            printf("(%s) device handle is NULL, Cannot get report descriptor\n", __func__);
         }
         else {
            int bytes_read = 0;
            bool ok = get_raw_report_descriptor(
                    dh,
                    bInterfaceNumber,
                    descriptor_len,              // report length
                    dbuf,
                    sizeof(dbuf),
                    &bytes_read);
            if (!ok)
               printf("(%s) get_raw_report_descriptor() returned %s\n", __func__, bool_repr(ok));
            if (ok) {
               puts("");
               rpt_hex_dump(dbuf, bytes_read, d1);
               puts("");
               report_retrieved_report_descriptor_and_probe(dh, dbuf, bytes_read, d1);
            }
         }
         break;
      }

      case LIBUSB_DT_STRING:
         printf("(%s) Unimplemented: String report descriptor\n", __func__);
         break;

      default:
         printf("(%s) Descriptor. Type= 0x%02x\n", __func__, cur->bDescriptorType);
         break;
      }
   }
   if (debug)
      printf("(%s) Done.\n", __func__);
}


//
// Module initialization
//

void init_libusb_reports() {
   devid_ensure_initialized();
}

