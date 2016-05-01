/* usb_core.h
 *
 * Created on: Apr 23, 2016
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

#ifndef SRC_USB_USB_CORE_H_
#define SRC_USB_USB_CORE_H_

#include "util/coredefs.h"
#include "base/displays.h"
#include "base/ddc_packets.h"
#include "ddc/vcp_feature_values.h"

Display_Info_List usb_get_valid_displays();

bool usb_is_valid_display_ref(Display_Ref * dref, bool emit_error_msg);

void usb_report_active_display_by_display_ref(Display_Ref * dref, int depth);

char * usb_get_capabilities_string_by_display_handle(Display_Handle * dh);

int usb_open_hiddev_device(char * hiddev_devname, bool emit_error_msg);
int usb_close_device(int fd, char * device_fn, Failure_Action failure_action);

Display_Ref * usb_find_display_by_model_sn(const char * model, const char * sn);

Display_Ref * usb_find_display_by_edid(const Byte * edidbytes);

Display_Ref *usb_find_display_by_busnum_devnum(int busnum, int devnum);

Parsed_Edid * usb_get_parsed_edid_by_display_ref(   Display_Ref    * dref);
Parsed_Edid * usb_get_parsed_edid_by_display_handle(Display_Handle * dh);

Global_Status_Code usb_get_nontable_vcp_value(
      Display_Handle *       dh,
      Byte                   feature_code,
      Parsed_Nontable_Vcp_Response** ppInterpretedCode);

Global_Status_Code usb_get_vcp_value(
       Display_Handle *          dh,
       Byte                      feature_code,
       Vcp_Value_Type            call_type,
       Single_Vcp_Value **       pvalrec);

Global_Status_Code usb_set_nontable_vcp_value(
       Display_Handle *       dh,
       Byte                   feature_code,
       int                    new_value);

Global_Status_Code
usb_set_vcp_value(                               // changed from set_vcp_value()
      Display_Handle *   dh,
      Single_Vcp_Value * vrec);

char * get_hiddev_devname_by_display_ref(Display_Ref * dref);

#endif /* SRC_USB_USB_CORE_H_ */
