/* ddc_vcp.h
 *
 * Created on: Jun 10, 2014
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

#ifndef DDC_VCP_H_
#define DDC_VCP_H_

#include <stdio.h>

#include "base/common.h"
#include "base/status_code_mgt.h"

#include "ddc/vcp_feature_codes.h"

Global_Status_Code put_vcp_by_display_ref(Display_Ref * pdisp, VCP_Feature_Table_Entry * vcp_entry, int new_value);

Global_Status_Code set_vcp_by_display_handle(Display_Handle * pDispHandle, Byte feature_code, int new_value);

Global_Status_Code get_table_vcp_by_display_handle(
       Display_Handle *       pDispHandle,
       Byte                   feature_code,
       Buffer**               pp_table_bytes);


Global_Status_Code get_vcp_by_display_handle(
       Display_Handle * pDispHandle,
       Byte feature_code,
       Interpreted_Vcp_Code** ppInterpretedCode);

Global_Status_Code get_vcp_by_display_ref(
       Display_Ref *          pDisp,
       Byte                   feature_code,
       Interpreted_Vcp_Code** ppInterpretedCode);


void vcp_list_feature_codes();

#endif /* DDC_VCP_H_ */
