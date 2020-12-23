/*
 * adl_mock_shim.c
 *
 * * <copyright>
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

/** \file
 * Mock implementation of ADL functions for use when **ddcutil**
 * is built without ADL support.  Thse functions satisfy the
 * dynamic linker.
 */

/** \cond */
#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stdlib.h>     // wchar_t, needed by adl_structures.h
/** \endcond */

#include "util/edid.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/execution_stats.h"
#include "base/status_code_mgt.h"

#include "adl/adl_shim.h"

// Initialization

bool            adl_debug;

/** Indicates if ADL support is available.
 *  Always returns **false**, since this is a mock
 *  implementation to satisfy the dynamic linker.
 *
 *  @retval false
 */
bool
adlshim_is_available()
{
   return false;
}

/** Mock implementation of ADL Initialization.
 * Always returns **false** to indicate failure.
 *
 * @retval false
 */
bool adlshim_initialize() {
   return false;
}



// Report on active displays


/** Mock implementation.
 * @retval NULL
 */
Parsed_Edid*
adlshim_get_parsed_edid_by_adlno(
      int iAdapterIndex,
      int iDisplayIndex)
{
   return NULL;
}

/** Mock implementation.
 * @retval NULL
 */
Parsed_Edid*
adlshim_get_parsed_edid_by_display_handle(
      Display_Handle * dh)
{
   return NULL;
}


/** Mock implementation.
 * @retval NULL
 */
Parsed_Edid*
adlshim_get_parsed_edid_by_dref(
      Display_Ref * dref)
{
   return NULL;
}


/** Mock implemention.  Does nothing. */
void
adlshim_report_active_display_by_dref(
      Display_Ref * dref,
      int depth) {
}


// Find and validate display

/** Mock implementation.
 * @retval false
 */
bool
adlshim_is_valid_display_ref(
      Display_Ref * dref,
      bool emit_error_msg)
{
   return false;
}

#ifdef OLD
/** Mock implementation.
 * @retval NULL
 */
Display_Ref *
adlshim_find_display_by_mfg_model_sn(
      const char * mfg_id,
      const char * model,
      const char * sn)
{
   return NULL;
}
#endif


/** Mock implementation.
 * @retval {-1,-1}
 */
DDCA_Adlno
adlshim_find_adlno_by_mfg_model_sn(
      const char * mfg_id,
      const char * model,
      const char * sn)
{
   DDCA_Adlno result = {-1,-1};
   return result;
}

#ifdef OLD
/** Mock implementation to satisfy dynamic linker.
 *
 * @param pEdidBytes  pointer to 128 byte EDID
 * @retval NULL
 */
Display_Ref * adlshim_find_display_by_edid(const Byte * pEdidBytes) {
   return NULL;
}
#endif

#ifdef OLD
/** Mock implementation to satisfy dynamic linker.
 * @return empty list
 */
Display_Info_List adlshim_get_valid_displays() {
   Display_Info_List info_list = {0,NULL};
   return info_list;
}
#endif


void adlshim_report_adl_display_detail(ADL_Display_Detail * detail, int depth) {
}

// new
int adlshim_get_valid_display_ct() {
   return 0;
}

// new
GPtrArray * adlshim_get_valid_display_details() {
   return g_ptr_array_new();
}



/** Mock implementation to satisfy dynamic linker.  Never called. */
Modulated_Status_ADL
adlshim_get_video_card_info(
      Display_Handle * dh,
      Video_Card_Info * card_info)
{
   assert(false);
   return 0;
}


// Read from and write to the display

/** Mock implementation to satisfy dynamic linker.  Never called. */
Modulated_Status_ADL
adlshim_ddc_write_only(
      Display_Handle* dh,
      Byte *  pSendMsgBuf,
      int     sendMsgLen) {
   assert(false);
   return 0;      // return code to avoid compile warning
}

/** Mock implementation to satisfy dynamic linker.  Never called. */
Modulated_Status_ADL
adlshim_ddc_read_only(
      Display_Handle* dh,
      Byte *  pRcvMsgBuf,
      int *   pRcvBytect) {
   assert(false);
   return 0;
}
