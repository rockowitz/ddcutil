/** \file ddc_vcp.c
 *  Virtual Control Panel access
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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


bool
ddc_set_verify_setvcp(
      bool                      onoff);

bool
ddc_get_verify_setvcp();

Error_Info *
ddc_save_current_settings(
      Display_Handle *          dh);

Error_Info *
ddc_set_nontable_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      int                       new_value);

Error_Info *
ddc_set_vcp_value(
      Display_Handle *          dh,
      DDCA_Any_Vcp_Value *        vrec,
      DDCA_Any_Vcp_Value **       newval_loc);

Error_Info *
ddc_get_table_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      Buffer**                  table_bytes_loc);

Error_Info *
ddc_get_nontable_vcp_value(
      Display_Handle *          dh,
      Byte                      feature_code,
      Parsed_Nontable_Vcp_Response** parsed_response_loc);

Error_Info *
ddc_get_vcp_value(
       Display_Handle *         dh,
       Byte                     feature_code,
       DDCA_Vcp_Value_Type      call_type,
       DDCA_Any_Vcp_Value **    valrec_loc);

void
init_ddc_vcp();

#endif /* DDC_VCP_H_ */
