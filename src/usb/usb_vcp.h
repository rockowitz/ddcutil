/* \file usb_vcp.h
 *
 * Get and set VCP feature codes for USB connected monitors.
 */

// Copyright (C) 2016-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef USB_VCP_H_
#define USB_VCP_H_

#include <linux/hiddev.h>

#include "ddcutil_types.h"

#include "util/coredefs.h"

#include "base/displays.h"
#include "base/ddc_packets.h"

#include "vcp/vcp_feature_values.h"


Public_Status_Code
usb_get_usage_value_by_report_type_and_ucode(
      int fd,
      __u32 report_type,
      __u32 usage_code,
      __s32 * maxval,
      __s32 * curval);

Public_Status_Code usb_get_nontable_vcp_value(
      Display_Handle *               dh,
      Byte                           feature_code,
      Parsed_Nontable_Vcp_Response** ppInterpretedCode);

Public_Status_Code usb_get_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      DDCA_Vcp_Value_Type            call_type,
      DDCA_Any_Vcp_Value **  pvalrec);

Public_Status_Code usb_set_nontable_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      int                       new_value);

Public_Status_Code usb_set_vcp_value(
      Display_Handle *           dh,
      DDCA_Any_Vcp_Value *  pvalrec);

__s32 usb_get_vesa_version(int fd);

#endif /* USB_VCP_H_ */
