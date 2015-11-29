/*
 * adl_intf.h
 *
 *  Created on: Jul 17, 2014
 *      Author: rock
 */

#ifndef ADL_INTF_H_
#define ADL_INTF_H_

#include <config.h>
#include <stdlib.h>     // wchar_t, needed by adl_structures.h
#include <stdbool.h>

#include "base/execution_stats.h"
#include "base/common.h"
#include "base/displays.h"
#include "base/edid.h"
#include "base/status_code_mgt.h"
#include "base/util.h"

#include "adl/adl_impl/adl_sdk_includes.h"

typedef
struct {
   int                   iAdapterIndex;
   int                   iDisplayIndex;
   bool                  supports_ddc;
   ADLDisplayEDIDData *  pAdlEdidData;
   ADLDDCInfo2 *         pAdlDDCInfo2;
   char                  mfg_id[4];
   char                  model_name[14];
   char                  serial_ascii[14];
   char                  xrandr_name[16];     // what is correct maximum size?
   Parsed_Edid *         pEdid;
} ADL_Display_Rec;


// Initialization

bool adl_is_available();
// must be called before any other function (except is_adl_available()):
bool adl_initialize();
void adl_release();


// Report on active displays

Parsed_Edid* adl_get_parsed_edid_by_adlno(int iAdapterIndex, int iDisplayIndex);

void adl_show_active_display(ADL_Display_Rec * pdisp, int depth);
void adl_show_active_display_by_index(int ndx, int depth);
void adl_show_active_display_by_adlno(int iAdapterIndex, int iDisplayIndex, int depth);
int  adl_show_active_displays();   // returns number of active displays

void report_adl_display_rec(ADL_Display_Rec * pRec, bool verbose, int depth);


// Find and validate display

bool              adl_is_valid_adlno(int iAdapterIndex, int iDisplayIndex, bool emit_error_msg);
ADL_Display_Rec * adl_get_display_by_adlno(int iAdapterIndex, int iDisplayIndex, bool emit_error_msg);
ADL_Display_Rec * adl_find_display_by_model_sn(const char * model, const char * sn);
ADL_Display_Rec * adl_find_display_by_edid(const Byte * pEdidBytes);

Display_Info_List adl_get_valid_displays();


// Read from and write to the display

Base_Status_ADL adl_ddc_write_only(
      int     iAdapterIndex,
      int     iDisplayIndex,
      Byte *  pSendMsgBuf,
      int     sendMsgLen);

Base_Status_ADL adl_ddc_read_only(
      int     iAdapterIndex,
      int     iDisplayIndex,
      Byte *  pRcvMsgBuf,
      int *   pRcvBytect);

Base_Status_ADL adl_ddc_write_read(
      int     iAdapterIndex,
      int     iDisplayIndex,
      Byte *  pSendMsgBuf,
      int     sendMsgLen,
      Byte *  pRcvMsgBuf,
      int *   pRcvBytect);

Base_Status_ADL adl_ddc_write_read_onecall(
      int     iAdapterIndex,
      int     iDisplayIndex,
      Byte *  pSendMsgBuf,
      int     sendMsgLen,
      Byte *  pRcvMsgBuf,
      int *   pRcvBytect);

#endif /* ADL_INTF_H_ */
