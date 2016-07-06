/* usb_core.h
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

#ifndef USB_CORE_H_
#define USB_CORE_H_

#include <linux/hiddev.h>     // for __u32

#include "util/coredefs.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/ddc_packets.h"

#include "vcp/vcp_feature_values.h"

#define REPORT_IOCTL_ERROR_AND_QUIT(_ioctl_name, _rc) \
   do { \
         printf("(%s) ioctl(%s) returned %d (0x%08x), errno=%d: %s\n", \
                __func__, \
                _ioctl_name, \
                _rc, \
                _rc, \
                errno, \
                strerror(errno) \
               ); \
         ddc_abort(errno); \
   } while(0)



bool check_usb_monitor( char * device_name );

Display_Info_List usb_get_valid_displays();

bool usb_is_valid_display_ref(Display_Ref * dref, bool emit_error_msg);

void usb_report_active_display_by_display_ref(Display_Ref * dref, int depth);

int usb_open_hiddev_device(char * hiddev_devname, Byte calloptions);
int usb_close_device(int fd, char * device_fn, Byte calloptions);

int hid_get_device_info(int fd, struct hiddev_devinfo *     dinfo, Byte calloptions);
int hid_get_report_info(int fd, struct hiddev_report_info * rinfo, Byte calloptions);
int hid_get_field_info( int fd, struct hiddev_field_info *  finfo, Byte calloptions);
int hid_get_usage_code( int fd, struct hiddev_usage_ref *   uref,  Byte calloptions);
int hid_get_usage_value(int fd, struct hiddev_usage_ref *   uref,  Byte calloptions);
int hid_get_report(     int fd, struct hiddev_report_info * rinfo, Byte calloptions);

Display_Ref * usb_find_display_by_model_sn(const char * model, const char * sn);
Display_Ref * usb_find_display_by_edid(const Byte * edidbytes);
Display_Ref * usb_find_display_by_busnum_devnum(int busnum, int devnum);

Parsed_Edid * usb_get_parsed_edid_by_display_ref(   Display_Ref    * dref);
Parsed_Edid * usb_get_parsed_edid_by_display_handle(Display_Handle * dh);



char * usb_get_capabilities_string_by_display_handle(Display_Handle * dh);

#ifdef NO_LONGER_NEEDED
char * get_hiddev_devnae_by_display_ref(Display_Ref * dref);
#endif


// struct defs here for sharing with usb_vcp

/* Used to record hiddev settings for reading and
 * writing a VCP feature code
 */
#define USB_MONITOR_VCP_REC_MARKER "UMVR"
typedef struct usb_monitor_vcp_rec {
   char                        marker[4];
   Byte                        vcp_code;
   __u32                       report_type;       // type?
   // have both indexes and struct pointers - redundant
   int                         report_id;
   int                         field_index;
   int                         usage_index;
   struct hiddev_report_info * rinfo;
   struct hiddev_field_info  * finfo;
   struct hiddev_usage_ref   * uref;
} Usb_Monitor_Vcp_Rec;


/* Describes a USB connected monitor.  */
#define USB_MONITOR_INFO_MARKER "UMIN"
typedef struct usb_monitor_info {
   char                     marker[4];
   char *                   hiddev_device_name;
   Parsed_Edid *            edid;
   struct hiddev_devinfo *  hiddev_devinfo;
   // a flagrant waste of space, avoid premature optimization
   GPtrArray *              vcp_codes[256];   // array of Usb_Monitor_Vcp_Rec *
} Usb_Monitor_Info;

Usb_Monitor_Info * usb_find_monitor_by_display_handle(Display_Handle * dh);

#endif /* USB_CORE_H_ */
