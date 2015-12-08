/*
 * adl_mock_shim.c
 *
 *  Created on: Nov 28, 2015
 *      Author: rock
 */

#include <assert.h>
#include <stdlib.h>     // wchar_t, needed by adl_structures.h
#include <stdbool.h>

#include "base/execution_stats.h"
#include "base/common.h"
#include "base/displays.h"
#include "base/edid.h"
#include "base/status_code_mgt.h"
#include "base/util.h"

#include "adl/adl_shim.h"

// Initialization

bool            adl_debug;

bool adlshim_is_available() {
   return false;
}

// must be called before any other function (except is_adl_available()):
bool adlshim_initialize() {
   return false;
}



// Report on active displays

Parsed_Edid* adlshim_get_parsed_edid_by_display_handle(Display_Handle * dh) {
   return NULL;
}


Parsed_Edid* adlshim_get_parsed_edid_by_display_ref(Display_Ref * dref) {
   return NULL;
}


void adlshim_show_active_display_by_display_ref(Display_Ref * dref, int depth) {
}


// Find and validate display

bool              adlshim_is_valid_display_ref(Display_Ref * dref, bool emit_error_msg) {
   return false;
}

Display_Ref * adlshim_find_display_by_model_sn(const char * model, const char * sn) {
   return NULL;
}

Display_Ref * adlshim_find_display_by_edid(const Byte * pEdidBytes) {
   return NULL;
}

Display_Info_List adlshim_get_valid_displays() {
   Display_Info_List info_list = {0,NULL};
   return info_list;
}

Global_Status_Code adlshim_get_video_card_info(
                      Display_Handle * dh,
                      Video_Card_Info * card_info) {
   return 0;
}



// Read from and write to the display

Global_Status_Code adlshim_ddc_write_only(
      Display_Handle* dh,
      Byte *  pSendMsgBuf,
      int     sendMsgLen) {
   assert(false);
   return 0;      // return code to avoid compile warning
}

Global_Status_Code adlshim_ddc_read_only(
      Display_Handle* dh,
      Byte *  pRcvMsgBuf,
      int *   pRcvBytect) {
   assert(false);
   return 0;
}

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
