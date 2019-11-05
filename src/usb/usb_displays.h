/** @file usb_displays.h
 */

// Copyright (C) 2016-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef USB_DISPLAYS_H_
#define USB_DISPLAYS_H_

/** \cond */
#include <glib.h>
#include <linux/hiddev.h>     // for __u32
/** \endcond */

#include "util/coredefs.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/ddc_packets.h"

#include "vcp/vcp_feature_values.h"

#include "usb/usb_base.h"


bool check_usb_monitor( char * device_name );

#ifdef OLD
Display_Info_List usb_get_valid_displays();
#endif

bool usb_is_valid_display_ref(Display_Ref * dref, bool emit_error_msg);

void usb_show_active_display_by_display_ref(Display_Ref * dref, int depth);

#ifdef PRE_DISPLAY_REV
Display_Ref * usb_find_display_by_mfg_model_sn(const char * mfg_id, const char * model, const char * sn);
Display_Ref * usb_find_display_by_edid(const Byte * edidbytes);
Display_Ref * usb_find_display_by_busnum_devnum(int busnum, int devnum);
#endif

Parsed_Edid * usb_get_parsed_edid_by_display_ref(   Display_Ref    * dref);
Parsed_Edid * usb_get_parsed_edid_by_display_handle(Display_Handle * dh);

char * usb_get_capabilities_string_by_display_handle(Display_Handle * dh);


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
#define USB_MONITOR_INFO_MARKER "UMNF"
typedef struct usb_monitor_info {
   char                     marker[4];
   char *                   hiddev_device_name;
   Parsed_Edid *            edid;
   struct hiddev_devinfo *  hiddev_devinfo;
   // a flagrant waste of space, avoid premature optimization
   GPtrArray *              vcp_codes[256];   // array of Usb_Monitor_Vcp_Rec *
} Usb_Monitor_Info;

void dbgrpt_usb_monitor_info(Usb_Monitor_Info * moninfo, int depth);

Usb_Monitor_Info * usb_find_monitor_by_display_handle(Display_Handle * dh);

bool is_possible_monitor_by_hiddev_name(const char * hiddev_name);

GPtrArray * get_usb_monitor_list();

void init_usb_displays();

#endif /* USB_DISPLAYS_H_ */
