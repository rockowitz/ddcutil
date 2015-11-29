/*  ddc_vcp.h
 *
 *  Created on: Jun 10, 2014
 *      Author: rock
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
