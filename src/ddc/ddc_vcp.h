/* ddc_vcp.h
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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
 */

#ifndef DDC_VCP_H_
#define DDC_VCP_H_

/** \cond */
#include <stdio.h>

#include "util/error_info.h"
/** \endcond */

#include "base/core.h"
#include "base/displays.h"
#include "base/status_code_mgt.h"

#include "vcp/vcp_feature_codes.h"
#include "vcp/vcp_feature_values.h"


void set_verify_setvcp(bool onoff);
bool get_verify_setvcp();

Error_Info *
save_current_settings(
      Display_Handle *          dh);

Error_Info *
set_nontable_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      int                       new_value);

Error_Info *
set_vcp_value(
      Display_Handle *          dh,
      Single_Vcp_Value *   vrec);

Error_Info *
get_table_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      Buffer**                  pp_table_bytes);

Error_Info *
get_nontable_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      Parsed_Nontable_Vcp_Response** pp_parsed_response);

Error_Info *
get_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      DDCA_Vcp_Value_Type       call_type,
      Single_Vcp_Value **  pvalrec);


#endif /* DDC_VCP_H_ */
