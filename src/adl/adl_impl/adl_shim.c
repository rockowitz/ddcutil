/*
 * adl_shim.c
 *
 *  Created on: Nov 28, 2015
 *      Author: rock
 */

#include <assert.h>
#include <stdlib.h>     // wchar_t, needed by adl_structures.h
#include <stdbool.h>

#include <base/execution_stats.h>
#include <base/common.h>
#include <base/displays.h>
#include <base/edid.h>
#include <base/status_code_mgt.h>
#include <base/util.h>

#include "adl/adl_impl/adl_intf.h"
#include "adl/adl_shim.h"

// Initialization

bool adlshim_is_available() {
   return adl_is_available();
}
// must be called before any other function (except is_adl_available()):
bool adlshim_initialize() {
   return adl_initialize();
}
void adlshim_release() {
   adl_release();
}


// Report on active displays

Parsed_Edid* adlshim_get_parsed_edid_by_display_handle(Display_Handle * dh) {
   assert(dh->ddc_io_mode == DDC_IO_ADL);
   return adl_get_parsed_edid_by_adlno(dh->iAdapterIndex, dh->iDisplayIndex);
}


Parsed_Edid* adlshim_get_parsed_edid_by_display_ref(Display_Ref * dref) {
   assert(dref->ddc_io_mode == DDC_IO_ADL);
   return adl_get_parsed_edid_by_adlno(dref->iAdapterIndex, dref->iDisplayIndex);
}

// needed?
void adlshim_show_active_display_by_adlno(int iAdapterIndex, int iDisplayIndex, int depth) {
   return adl_show_active_display_by_adlno(iAdapterIndex, iDisplayIndex, depth);
}

void adlshim_show_active_display_by_display_ref(Display_Ref * dref, int depth) {
   assert(dref->ddc_io_mode == DDC_IO_ADL);
   return adl_show_active_display_by_adlno(dref->iAdapterIndex, dref->iDisplayIndex, depth);
}


// Find and validate display

bool              adlshim_is_valid_display_ref(Display_Ref * dref, bool emit_error_msg) {
   assert(dref->ddc_io_mode == DDC_IO_ADL);
   return adl_is_valid_adlno(dref->iAdapterIndex, dref->iDisplayIndex, emit_error_msg);
}

Display_Ref * adlshim_find_display_by_model_sn(const char * model, const char * sn) {
   Display_Ref * dref = NULL;
   ADL_Display_Rec * adl_rec = adl_find_display_by_model_sn(model, sn);
   if (adl_rec)
      dref = create_adl_display_ref(adl_rec->iAdapterIndex, adl_rec->iDisplayIndex);
   return dref;
}

Display_Ref * adlshim_find_display_by_edid(const Byte * pEdidBytes) {
   Display_Ref * dref = NULL;
   ADL_Display_Rec * adl_rec = adl_find_display_by_edid(pEdidBytes);
   if (adl_rec)
      dref = create_adl_display_ref(adl_rec->iAdapterIndex, adl_rec->iDisplayIndex);
   return dref;
}

Display_Info_List adlshim_get_valid_displays() {
   return adl_get_valid_displays();
}



// Read from and write to the display

Global_Status_Code adlshim_ddc_write_only(
      Display_Handle* dh,
      Byte *  pSendMsgBuf,
      int     sendMsgLen)
{
   assert(dh->ddc_io_mode == DDC_IO_ADL);
   Base_Status_ADL adlrc = adl_ddc_write_only(dh->iAdapterIndex, dh->iDisplayIndex, pSendMsgBuf, sendMsgLen);
   Global_Status_Code gsc = modulate_rc(adlrc, RR_ADL);
   return gsc;
}

Global_Status_Code adlshim_ddc_read_only(
      Display_Handle* dh,
      Byte *  pRcvMsgBuf,
      int *   pRcvBytect)
{
   assert(dh->ddc_io_mode == DDC_IO_ADL);
   Base_Status_ADL adlrc = adl_ddc_read_only(dh->iAdapterIndex, dh->iDisplayIndex, pRcvMsgBuf, pRcvBytect);
   Global_Status_Code gsc = modulate_rc(adlrc, RR_ADL);
   return gsc;
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
