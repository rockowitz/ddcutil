/*
 * adl_shim.h
 *
 *  Created on: Nov 28, 2015
 *      Author: rock
 */

#ifndef ADL_SHIM_H_
#define ADL_SHIM_H_

#include <stdlib.h>     // wchar_t, needed by adl_structures.h
#include <stdbool.h>

#include "base/execution_stats.h"
#include "base/common.h"
#include "base/displays.h"
#include "base/edid.h"
#include "base/status_code_mgt.h"
#include "base/util.h"


// Initialization

bool adlshim_is_available();
// must be called before any other function (except is_adl_available()):
bool adlshim_initialize();
void adlshim_release();


// Report on active displays

Parsed_Edid* adlshim_get_parsed_edid_by_display_ref(Display_Ref * dref);

// void adl_show_active_display(ADL_Display_Rec * pdisp, int depth);
// void adl_show_active_display_by_index(int ndx, int depth);
void adlshim_show_active_display_by_adlno(int iAdapterIndex, int iDisplayIndex, int depth);
void adlshim_show_active_display_by_display_ref(Display_Ref * dref, int depth);
// int  adl_show_active_displays();   // returns number of active displays

// void report_adl_display_rec(ADL_Display_Rec * pRec, bool verbose, int depth);


// Find and validate display

bool              adlshim_is_valid_adlno(int iAdapterIndex, int iDisplayIndex, bool emit_error_msg);
bool              adlshim_is_valid_display_ref(Display_Ref * dref, bool emit_error_msg);
// ADL_Display_Rec * adl_get_display_by_adlno(int iAdapterIndex, int iDisplayIndex, bool emit_error_msg);
Display_Ref * adlshim_find_display_by_model_sn(const char * model, const char * sn);
Display_Ref * adlshim_find_display_by_edid(const Byte * pEdidBytes);

Display_Info_List adlshim_get_valid_displays();


// Read from and write to the display

Global_Status_Code adlshim_ddc_write_only(
      Display_Handle* dh,
      Byte *  pSendMsgBuf,
      int     sendMsgLen);

Global_Status_Code adlshim_ddc_read_only(
      Display_Handle * dh,
      Byte *  pRcvMsgBuf,
      int *   pRcvBytect);

//Base_Status_ADL adl_ddc_write_read(
//      int     iAdapterIndex,
//      int     iDisplayIndex,
//      Byte *  pSendMsgBuf,
//      int     sendMsgLen,
//      Byte *  pRcvMsgBuf,
//      int *   pRcvBytect);

//Base_Status_ADL adl_ddc_write_read_onecall(
//      int     iAdapterIndex,
//      int     iDisplayIndex,
//      Byte *  pSendMsgBuf,
//      int     sendMsgLen,
//      Byte *  pRcvMsgBuf,
//      int *   pRcvBytect);

#endif /* ADL_SHIM_H_ */
