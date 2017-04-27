/* ddc_vcp.h
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef DDC_VCP_H_
#define DDC_VCP_H_

#include <stdio.h>

#include "base/core.h"
#include "base/status_code_mgt.h"

#include "vcp/vcp_feature_codes.h"
#include "vcp/vcp_feature_values.h"


void set_verify_setvcp(bool onoff);
bool get_verify_setvcp();


Public_Status_Code
save_current_settings(
      Display_Handle * dh);

Public_Status_Code
set_nontable_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      int                       new_value);

Public_Status_Code
set_vcp_value(
      Display_Handle *          dh,
      DDCA_Single_Vcp_Value *   vrec);

Public_Status_Code
get_table_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      Buffer**                  pp_table_bytes);

Public_Status_Code
get_nontable_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
  //  bool                      retry_null_response,
      Parsed_Nontable_Vcp_Response** pp_parsed_response);

Public_Status_Code
get_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      DDCA_Vcp_Value_Type       call_type,
      DDCA_Single_Vcp_Value **  pvalrec);

#endif /* DDC_VCP_H_ */
