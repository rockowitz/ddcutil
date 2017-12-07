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
/** \endcond */

#include "base/core.h"
#include "base/retry_history.h"
#include "base/status_code_mgt.h"

#include "vcp/vcp_feature_codes.h"
#include "vcp/vcp_feature_values.h"


void set_verify_setvcp(bool onoff);
bool get_verify_setvcp();


Ddc_Error *
save_current_settings(
      Display_Handle *          dh);

Ddc_Error *
set_nontable_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      int                       new_value);

Ddc_Error *
set_vcp_value(
      Display_Handle *          dh,
      DDCA_Single_Vcp_Value *   vrec);

Ddc_Error *
get_table_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      Buffer**                  pp_table_bytes);

Ddc_Error *
get_nontable_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      Parsed_Nontable_Vcp_Response** pp_parsed_response);

Ddc_Error *
get_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      DDCA_Vcp_Value_Type       call_type,
      DDCA_Single_Vcp_Value **  pvalrec);

#endif /* DDC_VCP_H_ */
