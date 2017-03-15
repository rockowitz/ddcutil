/* adl_intf.c
 *
 * Interface to ADL (AMD Display Library) for fglrx
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
 *
 */

/** \cond */
#include <assert.h>
#include <dlfcn.h>            // dyopen, dysym, dyclose
#include <stdio.h>
#include <string.h>
#include <unistd.h>
/** \endcond */

#include "util/device_id_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "base/ddc_packets.h"
#include "base/parms.h"
#include "base/sleep.h"

#include "adl/adl_impl/adl_friendly.h"
#include "adl/adl_impl/adl_report.h"
#include "adl/adl_impl/adl_sdk_includes.h"

#include "adl/adl_impl/adl_intf.h"


#pragma GCC diagnostic ignored "-Wpointer-sign"


//
// ADL framework shim functions
//

// Memory allocation function
void* __stdcall ADL_Main_Memory_Alloc ( int iSize ) {
   // DBGMSG("iSize=%d", iSize );
   void* lpBuffer = malloc ( iSize );
   // DBGMSG("Returning: %p" , lpBuffer);
   return lpBuffer;
}

// Optional Memory de-allocation function
void __stdcall ADL_Main_Memory_Free ( void** pBuffer ) {
   // DBGMSG("pBuffer=%p, *pBuffer=%p  ", pBuffer, *pBuffer );
   if ( NULL != *pBuffer ) {
      free ( *pBuffer );
      *pBuffer = NULL;
   }
}

// Equivalent function in Linux:
void * GetProcAddress(void * pLibrary, const char * name) {
   return dlsym(pLibrary, name);
}


//
// Module globals
//

static Trace_Group TRACE_GROUP        = TRC_ADL;  // trace class for this file
static bool        module_initialized = false;
static bool        adl_linked         = false;;

// declared extern in adl_friendly.h, referenced in adl_services.c
bool          adl_debug          = false;

// declared extern in adl_friendly.h, referenced in adl_tests.c:
Adl_Procs*      adl;
ADL_Display_Rec active_displays[MAX_ACTIVE_DISPLAYS];
int             active_display_ct = 0;


//
// Call stats
//

#ifdef OLD
// dummy value for timing_stats in case init_adl_call_stats() is never called.
// Without it, macro RECORD_TIMING_STATS_NOERROR would have to test that
// both timing_stats and pTimingStat->p<stat> are not null.
static ADL_Call_Stats dummystats = {
        .pread_write_stats = NULL,
        .pother_stats      = NULL,
        .stats_active      = false
 };

static ADL_Call_Stats*  timing_stats = &dummystats;
static bool gather_timing_stats = false;

void init_adl_call_stats(ADL_Call_Stats * pstats) {
   // DBGMSG("Starting. pstats = %p", timing_stats);
   assert(pstats);
   timing_stats = pstats;
   gather_timing_stats = true;;

   // pstats->stat_group_name = "ADL call";
}
#endif


//
// Module initialization
//

/** Checks if the ADL environment has been initialized.
 *
 * @return true/false
 */
bool adl_is_available() {
   return (module_initialized);
}


/* Finds the ADL library and dynamically loads it
 *
 * Arguments:
 *   pLocAdl_Procs  return function table here
 *   verbose        if true, verbose messages
 *
 * Returns:
 *   0 loaded
 *   1 Library not found
 *  -1 error loading function
 */
static int link_adl(Adl_Procs** pLocAdl_Procs, bool verbose) {
   bool debug = false;
   DBGMSF(debug, "Starting." );
   Adl_Procs* adl =  calloc(1, sizeof(Adl_Procs));
   int result = 0;

#define LOADFUNC(_n_) \
   do { \
   DBGMSF(debug, "Loading function %s", #_n_);    \
   adl->_n_ = dlsym(adl->hDLL, #_n_); \
   if (!adl->_n_) { \
       SEVEREMSG("ADL error: loading symbol %s\n", #_n_); \
       result = -1; \
   }   \
   } while (0)


   adl->hDLL = dlopen("libatiadlxx.so", RTLD_LAZY|RTLD_GLOBAL);
   if (!adl->hDLL) {
      if (verbose)
         printf("ADL library libatiadlxx.so not found.\n" );   // this is a user error msg
      result = 1;
   }
   else {
       LOADFUNC(ADL_Main_Control_Create);
       LOADFUNC(ADL_Main_Control_Destroy);

       LOADFUNC(ADL_Adapter_NumberOfAdapters_Get);
       LOADFUNC(ADL_Adapter_AdapterInfo_Get);
       LOADFUNC( ADL_Adapter_VideoBiosInfo_Get );
       LOADFUNC(ADL2_Adapter_VideoBiosInfo_Get );
       LOADFUNC(ADL_Display_NumberOfDisplays_Get);
       LOADFUNC(ADL_Display_DisplayInfo_Get);

       LOADFUNC(ADL_Display_ColorCaps_Get   );
       LOADFUNC(ADL_Display_Color_Get );
       LOADFUNC(ADL_Display_Color_Set );
       LOADFUNC(ADL2_Display_ColorCaps_Get   );
       LOADFUNC(ADL2_Display_Color_Get );
       LOADFUNC(ADL2_Display_Color_Set );

       // I2C, DDC, and EDID APIs
       LOADFUNC(ADL2_Display_WriteAndReadI2CRev_Get );
       LOADFUNC( ADL_Display_WriteAndReadI2CRev_Get );
       LOADFUNC(ADL2_Display_WriteAndReadI2C );
       LOADFUNC( ADL_Display_WriteAndReadI2C );
       LOADFUNC(ADL2_Display_DDCBlockAccess_Get);
       LOADFUNC( ADL_Display_DDCBlockAccess_Get);
       LOADFUNC(ADL2_Display_DDCInfo_Get  );
       LOADFUNC( ADL_Display_DDCInfo_Get  );
       LOADFUNC(ADL2_Display_DDCInfo2_Get );
       LOADFUNC( ADL_Display_DDCInfo2_Get );
       LOADFUNC(ADL2_Display_EdidData_Get );
       LOADFUNC( ADL_Display_EdidData_Get );

       // Linux only APIs
       LOADFUNC(ADL2_Adapter_XScreenInfo_Get       );
       LOADFUNC( ADL_Adapter_XScreenInfo_Get       );
       LOADFUNC(ADL2_Display_XrandrDisplayName_Get );
       LOADFUNC( ADL_Display_XrandrDisplayName_Get );

   }
   DBGMSF(debug, "adl->ADL_Main_Control_Create = %p   ", adl->ADL_Main_Control_Create );
   if (result == 0) {
      *pLocAdl_Procs = adl;
   }
   else {
      free(adl);
      *pLocAdl_Procs = NULL;
   }
   DBGMSF(debug, "Returning %d, *pLocAdl_Procs=%p   ", result, *pLocAdl_Procs ) ;
   return result;

#undef LOADFUNC
}



/* Initialize the ADL framework.
 *
 * Returns:
 *    true if successful, false if not
 */
static bool init_framework() {
   bool ok = true;
   int rc;

   // Initialize ADL. The second parameter is 1, which means:
   // retrieve adapter information only for adapters that are physically present and enabled in the system
   if (adl_debug) {
      DBGMSG("adl=%p", adl );
      DBGMSG("adl->ADL_Main_Control_Create=%p   ", adl->ADL_Main_Control_Create );
   }

   RECORD_IO_EVENT(
      IE_OTHER,
      (
         rc = adl->ADL_Main_Control_Create(ADL_Main_Memory_Alloc, 1)
      )
   );

   // rc = adl->ADL_Main_Control_Create(ADL_Main_Memory_Alloc, 1) ;
   if (rc != ADL_OK) {
      if (rc == ADL_ERR_NO_XDISPLAY) {
         fprintf(stderr, "No X display found by ADL. Apparently running in console environment. (ADL_ERR_NO_XDISPLAY)\n");
      }
      else
         fprintf(stderr, "ADL Initialization Error! ADL_Main_Control_Create() returned: %d.\n", rc);
      ok = false;
   }
   return ok;
}


#ifdef UNUSED
bool isConnectedAndMapped(ADLDisplayInfo * pDisplayInfo) {
   // DBGMSG("Startimg.  " );
   bool result = true;

   // Use the display only if it's connected AND mapped (iDisplayInfoValue: bit 0 and 1 )
   if (  ( ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED  | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED ) !=
         ( (ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED) &
           pDisplayInfo->iDisplayInfoValue )
      )
   {
      // DBGMSG("Display not connected or not mapped" );
      result = false;  // Skip the not connected or not mapped displays
   }

   // DBGMSG("Returning %d   ", result );
   return result;
}
#endif


static bool is_active_display(int iAdapterIndex, ADLDisplayInfo * pDisplayInfo) {
   // DBGMSG("Startimg.  " );
   bool result = true;

   // Use the display only if it's connected AND mapped (iDisplayInfoValue: bit 0 and 1 )
   if (  ( ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED  | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED ) !=
         ( (ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED) &
           pDisplayInfo->iDisplayInfoValue )
      )
   {
      // DBGMSG("Display not connected or not mapped" );
      result = false;  // Skip the not connected or not mapped displays
   }

   // Is the display mapped to this adapter? This test is too restrictive and may not be needed.
   // Appears necessary (SAR) - otherwise get additional displays
   else if ( iAdapterIndex != pDisplayInfo->displayID.iDisplayLogicalAdapterIndex ) {
      // DBGMSG("Display not mapped to this adapter   " );
      result = false;
   }

   // DBGMSG("Returning %d   ", result );
   return result;
}


/* Scan for attached displays
 *
 * Information about displays found is accumulated in global variables
 * active_displays and active_display_ct.
 *
 * Returns:
 *    true  if successful, false if not
 */
static bool scan_for_displays() {
   bool debug = false;
   if (adl_debug)
      debug = true;
   DBGMSF(debug, "Starting." );
   int            rc;
   int            iNumberAdapters;
   AdapterInfo *  pAdapterInfo;
   bool           ok = true;

   // Obtain the number of adapters for the system
   RECORD_IO_EVENT(
      IE_OTHER,
      (
         rc = adl->ADL_Adapter_NumberOfAdapters_Get ( &iNumberAdapters )
      )
   );
   if (rc != ADL_OK) {
      DBGMSG("Cannot get the number of adapters!  ADL_Adapter_NumberOfAdapaters_Get() returned %d", rc);
      ok = false;
   }
   // DBGMSG("Found %d adapters", iNumberAdapters );

   if ( 0 < iNumberAdapters ) {
      pAdapterInfo = malloc ( sizeof (AdapterInfo) * iNumberAdapters );
      memset( pAdapterInfo,'\0', sizeof (AdapterInfo) * iNumberAdapters );

      // Get the AdapterInfo structure for all adapters in the system
      RECORD_IO_EVENT(
         IE_OTHER,
         (
            adl->ADL_Adapter_AdapterInfo_Get(pAdapterInfo, sizeof (AdapterInfo) * iNumberAdapters)
         )
      );

      // Repeat for all available adapters in the system
      int adapterNdx, displayNdx;   // counters for looping
      int iAdapterIndex;
      int iDisplayIndex;
      int displayCtForAdapter;
      ADLDisplayInfo * pAdlDisplayInfo = NULL;

      for (adapterNdx = 0; adapterNdx < iNumberAdapters; adapterNdx++ ) {
         // printf("Adapter loop index: %d\n", adapterNdx );
         AdapterInfo * pAdapter = &pAdapterInfo[adapterNdx];
         if (adl_debug)
            report_adl_AdapterInfo(pAdapter, 1);
         iAdapterIndex = pAdapter->iAdapterIndex;
         assert(iAdapterIndex == adapterNdx);    // just an observation

#ifdef REFERENCE
         typedef struct ADLBiosInfo
         {
            char strPartNumber[ADL_MAX_PATH];   ///< Part number.
            char strVersion[ADL_MAX_PATH];      ///< Version number.
            char strDate[ADL_MAX_PATH];      ///< BIOS date in yyyy/mm/dd hh:mm format.
         } ADLBiosInfo, *LPADLBiosInfo;
#endif
#ifdef WORKS_BUT_NOT_VERY_USEFUL_INFO
         ADLBiosInfo adlBiosInfo;

         RECORD_IO_EVENT(
         IE_OTHER,
         (
            rc = adl->ADL_Adapter_VideoBiosInfo_Get (
                         iAdapterIndex,
                         &adlBiosInfo)       // fill in the struct
         )
        );
         if (rc != ADL_OK) {
             DBGMSG("ADL_Adapter_VideoBiosInfo_Get() returned %d", rc);
             continue;
          }
         else {    // TEMP
            // not very useful info, is it even worth saving in ADL_Display_Rec?
            // printf("VideoBiosInfo\n");
            // printf("   PartNumber: %s\n", adlBiosInfo.strPartNumber);
            // printf("   Version:    %s\n", adlBiosInfo.strVersion);
            // printf("   Date:       %s\n", adlBiosInfo.strDate);

         }
#endif

         pAdlDisplayInfo = NULL;    // set to NULL before calling ADL_Display_DisplayInfo_Get()
         RECORD_IO_EVENT(
            IE_OTHER,
            (
               rc = adl->ADL_Display_DisplayInfo_Get (
                            iAdapterIndex,
                            &displayCtForAdapter,   // return number of displays detected here
                            &pAdlDisplayInfo,       // return pointer to retrieved displayinfo array here
                            0)                      // do not force detection
            )
         );
         if (rc != ADL_OK) {
            DBGMSG("ADL_Display_DisplayInfo_Get() returned %d", rc);
            continue;
         }
         // DBGMSG("ADL_Display_DisplayInfo_Get() succeeded, displayCtForAdapter=%d", displayCtForAdapter );

         for ( displayNdx = 0; displayNdx < displayCtForAdapter; displayNdx++ ) {
            // DBGMSG("adapter loop index = %d,   display loop index = %d  ", adapterNdx, displayNdx);
            ADLDisplayInfo * pCurDisplayInfo = &pAdlDisplayInfo[displayNdx];
            iDisplayIndex = pCurDisplayInfo->displayID.iDisplayLogicalIndex;
            // DBGMSG("iAdapterIndex=%d, iDisplayIndex=%d", iAdapterIndex, iDisplayIndex );
            if (debug) {
               DBGMSG("iAdapterIndex=%d, iDisplayIndex=%d", iAdapterIndex, iDisplayIndex );
               report_adl_ADLDisplayInfo(pCurDisplayInfo, 2);
            }
            char xrandrname[100] = {0};
            RECORD_IO_EVENT(
               IE_OTHER,
               (
                  rc = adl->ADL_Display_XrandrDisplayName_Get(
                               iAdapterIndex, iDisplayIndex, xrandrname, 100)
               )
            );
            if (rc != 0)
               DBGMSG("ADL_Display_XrandrDisplayName_Get() returned %d\n   ", rc );
            // if (rc == 0)
            //    DBGMSG("ADL_Display_XrandrDisplayName_Get returned xrandrname=|%s|", xrandrname );

            if (is_active_display(iAdapterIndex, pCurDisplayInfo)) {
            // if (isConnectedAndMapped(pCurDisplayInfo) ) {
               // DBGMSG("Found active display.  iAdapterIndex=%d, iDisplayIndex=%d", iAdapterIndex, iDisplayIndex );
               assert(active_display_ct < MAX_ACTIVE_DISPLAYS);
               ADL_Display_Rec * pCurActiveDisplay = &active_displays[active_display_ct];
               pCurActiveDisplay->iAdapterIndex = iAdapterIndex;
               pCurActiveDisplay->iDisplayIndex = iDisplayIndex;

               pCurActiveDisplay->iVendorID = pAdapter->iVendorID;
               pCurActiveDisplay->pstrAdapterName = strdup(pAdapter->strAdapterName);
               pCurActiveDisplay->pstrDisplayName = strdup(pAdapter->strDisplayName);

               ADLDisplayEDIDData * pEdidData = calloc(1, sizeof(ADLDisplayEDIDData));

               pEdidData->iSize = sizeof(ADLDisplayEDIDData);
               pEdidData->iFlag = 0;
               pEdidData->iBlockIndex = 0;   // critical

               RECORD_IO_EVENT(
                  IE_OTHER,
                  (
                     rc = adl->ADL_Display_EdidData_Get(iAdapterIndex, iDisplayIndex, pEdidData)
                  )
               );
               if (rc != ADL_OK) {
                  DBGMSG("ADL_Display_EdidData_Get() returned %d", rc );
                  pCurActiveDisplay->pAdlEdidData = NULL;
                  free(pEdidData);
               }
               else {
                  // puts("EdidData_Get succeeded");
                  // reportDisplayEDIDData(pEdidData,3);
                  pCurActiveDisplay->pAdlEdidData = pEdidData;
                  Byte * pEdidBytes = (Byte *) &(pEdidData->cEDIDData);
                  Parsed_Edid * pEdid = create_parsed_edid(pEdidBytes);
                  memcpy(pCurActiveDisplay->mfg_id,       pEdid->mfg_id,       sizeof(pCurActiveDisplay->mfg_id));
                  memcpy(pCurActiveDisplay->model_name,   pEdid->model_name,   sizeof(pCurActiveDisplay->model_name));
                  memcpy(pCurActiveDisplay->serial_ascii, pEdid->serial_ascii, sizeof(pCurActiveDisplay->serial_ascii));
                  // should use snprintf to ensure no buffer overflow:
                  memcpy(pCurActiveDisplay->xrandr_name,  xrandrname,          sizeof(pCurActiveDisplay->xrandr_name));
                  pCurActiveDisplay->pEdid = pEdid;
               }

               ADLDDCInfo2 * pDDCInfo2 = calloc(1, sizeof(ADLDDCInfo2));
               RECORD_IO_EVENT(
                  IE_OTHER,
                  (
                     rc = adl->ADL_Display_DDCInfo2_Get(iAdapterIndex, iDisplayIndex, pDDCInfo2)
                  )
               );
               if (rc != ADL_OK) {
                  DBGMSG("ADL_Display_DDCInfo2_Get() returned %d", rc );
                  pCurActiveDisplay->pAdlDDCInfo2 = NULL;
                  free(pDDCInfo2);
                  pCurActiveDisplay->supports_ddc = false;
               }
               else {
                  // DBGMSG("ADL_DISPLAY_DDCInfo2_Get succeeded");
                  // report_adl_ADLDDCInfo2(pDDCInfo2, false /* verbose */, 3);
                  pCurActiveDisplay->pAdlDDCInfo2 = pDDCInfo2;

                  // This is less useful than the name suggests.
                  // Dell 1905FP does not support DDC, but returns true.
                  // Needs further checking to confirm that DDC supported
                  pCurActiveDisplay->supports_ddc = (pDDCInfo2->ulSupportsDDC) ? true : false;
               }

               active_display_ct++;

            }  // is_active_display

         }   // iNumDisplays
         free(pAdlDisplayInfo);
         pAdlDisplayInfo = NULL;
      }      // iNumberAdapters

   }  // iNumberAdapters > 0

   DBGMSF(debug, "Returning %d", ok );
   return ok;
}



/** This is the main function for initializing the ADL environment.
 *
 * Must be called before any function other than is_adl_available().
 *
 * It is not an error if this function is called multiple times.
 *
 * Performs the following steps:
 * - checks if ADL tracing is in effect
 * - dynamically links the ADL library
 * - initializes the framework
 * - scans for ADL monitors
 *
 * @retval true  if initialization succeeded, or already initialized
 * @retval false initialization failed
 *
 * Returns:
 *   true if initialization successful, false if not
 */
bool adl_initialize() {
   if (module_initialized) {
      return true;
   }
   // A hack, to reuse all the if (debug) ... tracing code without converting
   // it to use TRCMSG.  The debug flag is made global to the module.
   adl_debug = IS_TRACING();
   // DBGMSG("adl_debug=%d", adl_debug);

   int rc;
   bool ok = false;

   rc = link_adl(&adl, false /* verbose */);
   if (rc != 0 && adl_debug)
      DBGMSG("link_adl() failed   " );
   if (rc == 0) {
      adl_linked = true;
      ok = init_framework();
      if (ok) {
         ok = scan_for_displays();
         // DBGMSG("init_adl_new() returned %d", ok );
      }
   }
   if (ok) {
      module_initialized = true;
   }

   return ok;
}

/** Releases the ADL framework
 */
void adl_release() {
   RECORD_IO_EVENT(
      IE_OTHER,
      (
         adl->ADL_Main_Control_Destroy()
      )
   );
   // adl->ADL_Main_Control_Destroy();
   dlclose(adl->hDLL);
   free(adl);
   module_initialized = false;
}


//
// Report on active displays
//

/** Returns a #Parsed_Edid for an ADL display
 *
 * @param  iAdapterIndex  adapter number
 * @param  iDisplayIndex  display number
 * @return pointer to #Parsed_Edid, NULL if display not found
 */
Parsed_Edid*
adl_get_parsed_edid_by_adlno(
      int iAdapterIndex,
      int iDisplayIndex)
{
   Parsed_Edid* parsedEdid = NULL;
   ADL_Display_Rec * pAdlRec = adl_get_display_by_adlno(
                                   iAdapterIndex,
                                   iDisplayIndex,
                                   false);
   if (pAdlRec) {
      parsedEdid = pAdlRec->pEdid;
   }
   return parsedEdid;
}


#ifdef DEPRECATED
// use get_ParsedEdid_for_adlno
DisplayIdInfo* get_adl_display_id_info(int iAdapterIndex, int iDisplayIndex) {
   DisplayIdInfo * pIdInfo = NULL;

   Parsed_Edid * pEdid = adl_get_parsed_edid_by_adlno(iAdapterIndex, iDisplayIndex);
   if (pEdid) {
      pIdInfo = calloc(1, sizeof(DisplayIdInfo));
      memcpy(pIdInfo->mfg_id,       pEdid->mfg_id,       sizeof(pIdInfo->mfg_id));
      memcpy(pIdInfo->model_name,   pEdid->model_name,   sizeof(pIdInfo->model_name));
      memcpy(pIdInfo->serial_ascii, pEdid->serial_ascii, sizeof(pIdInfo->serial_ascii));
      memcpy(pIdInfo->edid_bytes,   pEdid->bytes,        128);
   }
   return pIdInfo;
}
#endif


/** Describes a display, specified by a pointer to its ADL_Display_Rec,
 *  using report functions.
 *
 * Output is written to the currently active report destination.
 *
 * @param   pdisp      points to ADL_Display_Rec for the display
 * @param   depth      logical indentation depth
 */
void adl_report_active_display(ADL_Display_Rec * pdisp, int depth) {
   DDCA_Output_Level output_level = get_output_level();
   rpt_vstring(depth, "ADL Adapter number:   %d", pdisp->iAdapterIndex);
   rpt_vstring(depth, "ADL Display number:   %d", pdisp->iDisplayIndex);
   // Can be true even if doesn't support DDC, e.g. Dell 1905FP
   // avoid confusion - do not display
   // if (output_level >= OL_VERBOSE)
   //    rpt_vstring(depth, "Supports DDC:         %s", (pdisp->supports_ddc) ?  "true" : "false");
   if (output_level == OL_TERSE)
   rpt_vstring(depth, "Monitor:              %s:%s:%s", pdisp->mfg_id, pdisp->model_name, pdisp->serial_ascii);
   rpt_vstring(depth, "Xrandr name:          %s", pdisp->xrandr_name);
   if (output_level >= OL_NORMAL) {
      bool dump_edid = (output_level >= OL_VERBOSE);
      report_parsed_edid(pdisp->pEdid, dump_edid /* verbose */, depth);
   }
   if (output_level >= OL_VERBOSE) {
      devid_ensure_initialized();
      Pci_Usb_Id_Names pci_id_names = devid_get_pci_names((ushort) pdisp->iVendorID, 0, 0, 0, 1);
      char * vendor_name = (pci_id_names.vendor_name) ? pci_id_names.vendor_name : "unknown vendor";
      rpt_vstring(depth, "Vendor id:            0x%04x  %s", pdisp->iVendorID, vendor_name);
      // waste of space to reserve full ADL_MAX_PATH for each field
      if (pdisp->pstrAdapterName)
      rpt_vstring(depth, "Adapter name:         %s", pdisp->pstrAdapterName);
      if (pdisp->pstrDisplayName)
      rpt_vstring(depth, "Display name:         %s", pdisp->pstrDisplayName);
   }
}


/** Describes a display, specified by its index in the list of displays,
 * using report functions.
 *
 * Output is written to the currently active report destination.
 *
 * @param   ndx        index into array of active ADL displays
 * @param   depth      logical indentation depth
 */
void adl_report_active_display_by_index(
        int ndx,
        int depth)
{
   assert(ndx >= 0 && ndx < active_display_ct);
   ADL_Display_Rec * pdisp = &active_displays[ndx];
   adl_report_active_display(pdisp, depth);
}


/* Describes a display, sepecified by an adapter number/display number pair,
 * using report functions.
 *
 * Output is written to the currently active report destination.
 *
 * @param   iAdapterIndex  adapter number
 * @param   iDisplayIndex  display number for adapter
 * @param   depth          logical indentation depth
 */
void adl_report_active_display_by_adlno(
        int iAdapterIndex,
        int iDisplayIndex,
        int depth)
{
   ADL_Display_Rec * pdisp = adl_get_display_by_adlno(iAdapterIndex, iDisplayIndex, false /* emit_error_msg */);
   if (!pdisp)
      rpt_vstring(depth, "ADL display %d.%d not found", iAdapterIndex, iDisplayIndex);
   else
      adl_report_active_display(pdisp, depth);
}


/** Show information about attached ADL displays.
 *
 * Output is written using report functions.
 *
 * @return number of active displays
 */
int adl_report_active_displays() {
   if (adl_linked) {
      rpt_vstring(0, "\nDisplays connected to AMD proprietary driver: %s",
                     (active_display_ct == 0) ? "None" : "");
      rpt_vstring(0, "");
      if (active_display_ct > 0) {
         int ndx;
         for (ndx=0; ndx < active_display_ct; ndx++) {
            ADL_Display_Rec * pdisp = &active_displays[ndx];
            adl_report_active_display(pdisp, 0);
            rpt_vstring(0,"");
         }
      }
   }
   return active_display_ct;
}


/** Returns a #Display_Info_List describing the detected ADL displays.
 *
 *  @return #Display_Info_List
 */
Display_Info_List adl_get_valid_displays() {
   Display_Info_List info_list = {0,NULL};
   Display_Info * info_recs = calloc(active_display_ct, sizeof(Display_Info));

   int ndx = 0;
   for (ndx=0; ndx < active_display_ct; ndx++) {
      ADL_Display_Rec * pdisp = &active_displays[ndx];
      info_recs[ndx].dref = create_adl_display_ref(pdisp->iAdapterIndex, pdisp->iDisplayIndex);
      info_recs[ndx].edid = pdisp->pEdid;
      memcpy(info_recs[ndx].marker, DISPLAY_INFO_MARKER, 4);
   }
   info_list.info_recs = info_recs;
   info_list.ct = active_display_ct;
   // DBGMSG("Done. Returning:");
   // report_display_info_list(&info_list, 0);
   return info_list;
}


#ifdef REFERENCE
typedef struct {
   Display_Ref * dref;
   Parsed_Edid * edid;
} Display_Info;

typedef struct {
   int ct;
   Display_Info * info_recs;
} Display_Info_List;

#endif


/* Reports the contents of a #ADL_Display_Rec, describing a single active display.
 * This is a debugging functions.
 *
 * Output is written using report functions.
 *
 * @param  pRec     pointer to display record
 * @param  verbose  if true, show additional detail
 * @param  depth    logical indentation depth
 */
void report_adl_display_rec(
        ADL_Display_Rec * pRec,
        bool              verbose,
        int               depth)
{
   verbose=false;

   rpt_structure_loc("AdlDisplayRec", pRec, depth);
   int d = depth+1;
   rpt_int( "iAdapterIndex",  NULL,                       pRec->iAdapterIndex,   d);
   rpt_int( "iDisplayIndex",  NULL,                       pRec->iDisplayIndex,   d);
   rpt_int( "supportsDDC",    "does display support DDC", pRec->supports_ddc,    d);
   rpt_str( "mfg_id",         "manufacturer id",          pRec->mfg_id,          d);
   rpt_str( "model_name",     NULL,                       pRec->model_name,      d);
   rpt_str( "serial_ascii",   NULL,                       pRec->serial_ascii,    d);
   rpt_int( "iVendorID",      "vendor id (as decimal)",   pRec->iVendorID,       d);
   rpt_str( "strAdapterName", "video card name",          pRec->pstrAdapterName, d);
   rpt_str( "pstrDisplayName", NULL,                      pRec->pstrDisplayName, d);

   if (verbose) {
      report_adl_ADLDisplayEDIDData(pRec->pAdlEdidData, d+1);
      report_adl_ADLDDCInfo2(pRec->pAdlDDCInfo2, false /* verbose */, d+1);
   }
}

#ifdef REF
int                   iVendorID;                       // e.g. 4098
// waste of space to reserve full ADL_MAX_PATH for each field
char *                pstrAdapterName;
char *                pstrDisplayName;
#endif


/** Gets video card information for the specified adapter number/display number pair.
 *
 *  @param  iAdapterIndex  adapter number
 *  @param  iDisplayIndex  display number
 *  @param  card_info      pointer to existing #Video_Card_Info struct where
 *                         information is returned
 *
 *  @return ADL status code
 *
 *  @todo
 *  A pointer to a newly allocated string for the adapter name is returned
 *  in card_info.  This is a memory leak.
 */
Base_Status_ADL
adl_get_video_card_info_by_adlno(
      int               iAdapterIndex,
      int               iDisplayIndex,
      Video_Card_Info * card_info)
{
   assert( card_info != NULL && memcmp(card_info->marker, VIDEO_CARD_INFO_MARKER, 4) == 0);
   int rc = 0;
   ADL_Display_Rec * pAdlRec = adl_get_display_by_adlno(iAdapterIndex, iDisplayIndex, true /* emit_error_msg */);
   if (!pAdlRec) {
      PROGRAM_LOGIC_ERROR("%s called with invalid Display_Handle");
   }
   // assignments wrapped in unnecessary else{} clause to avoid clang warning
   // about possible null dereference in case pAdlRec is NULL
   else {
      card_info->vendor_id    = pAdlRec->iVendorID;
      card_info->adapter_name = strdup(pAdlRec->pstrAdapterName);
      card_info->driver_name  = "AMD proprietary driver";
   }
   return rc;
}




//
// Find display
//

/** Finds ADL display by adapter number/display number
 *
 *  @param  iAdapterIndex   ADL adapter number
 *  @param  iDisplayIndex   ADLdisplay number
 *  @param  emit_error_msg  if true, emit messages for errors
 *
 *  @return pointer to #ADL_Display_Rec describing the display,
 *          NULL if not found
 */
ADL_Display_Rec *
adl_get_display_by_adlno(
      int  iAdapterIndex,
      int  iDisplayIndex,
      bool emit_error_msg)
{
   ADL_Display_Rec * result = NULL;

   if (active_display_ct == 0) {
      if (emit_error_msg)
         f0printf(FERR, "No ADL displays found\n");
   }
   else {
      int ndx;
      for (ndx = 0; ndx < active_display_ct; ndx++) {
         ADL_Display_Rec * pdisp = &active_displays[ndx];
         if (iAdapterIndex == pdisp->iAdapterIndex &&
             iDisplayIndex == pdisp->iDisplayIndex )
         {
            result = pdisp;
            break;
         }
      }
      if (!result && emit_error_msg)
         f0printf(FERR, "ADL display %d.%d not found.\n", iAdapterIndex, iDisplayIndex);
   }

   return result;
}


/** Finds ADL display by some combination of manufacturer id, model name and serial number
 *
 * @param  mfg_id   3 character manufacturer id
 * @param  model    model name string
 * @param  sn       serial number string
 *
 *  @return pointer to #ADL_Display_Rec describing the display,
 *          NULL if not found
 */
ADL_Display_Rec *
adl_find_display_by_mfg_model_sn(
      const char * mfg_id,
      const char * model,
      const char * sn)
{
   // DBGMSG("Starting. mode=%s, sn=%s   ", model, sn );
   ADL_Display_Rec * result = NULL;
   bool some_test_passed = false;
   bool some_test_failed = false;
   int ndx;
   for (ndx = 0; ndx < active_display_ct; ndx++) {
      ADL_Display_Rec * pdisp = &active_displays[ndx];
      if ( mfg_id && strlen(mfg_id) > 0) {
         if ( streq(mfg_id, pdisp->mfg_id) )
            some_test_passed = true;
         else
            some_test_failed = true;
      }
      if ( model && strlen(model) > 0) {
         if ( streq(model, pdisp->model_name) )
            some_test_passed = true;
         else
            some_test_failed = true;
      }
      if ( sn && strlen(sn) > 0) {
         if ( streq(sn, pdisp->serial_ascii) )
            some_test_passed = true;
         else
            some_test_failed = true;
      }

      if (some_test_passed && !some_test_failed)

      // if (streq(model,pdisp->model_name) &&
      //     streq(sn, pdisp->serial_ascii) )
      {
         result = pdisp;
         break;
      }
   }
   // DBGMSG("Returning: %p   ", result );
   return result;
}


/** Finds ADL display by EDID
 *
 *  @param  pEdidBytes pointer to 128 byte EDID
 *
 *  @return pointer to #ADL_Display_Rec describing the display,
 *          NULL if not found
 */
ADL_Display_Rec *
adl_find_display_by_edid(
      const Byte * pEdidBytes)
{
   // DBGMSG("Starting. mode=%s, sn=%s   ", model, sn );
   ADL_Display_Rec * result = NULL;
   int ndx;
   for (ndx = 0; ndx < active_display_ct; ndx++) {
      ADL_Display_Rec * pdisp = &active_displays[ndx];
      // need to check AdlEdidData fields for valid data present?
      if ( memcmp(pEdidBytes, pdisp->pAdlEdidData->cEDIDData, 128) == 0  )
      {
         result = pdisp;
         break;
      }
   }
   // DBGMSG("Returning: %p   ", result );
   return result;
}


/** Verifies that an ( adapter number,display number) pair specifies an active ADL display
 *
 *  @param  iAdapterIndex  ADL adapter number
 *  @param  iDisplayIndex  ADL display number
 *  @param  emit_error_msg if true, emit error messages
 *
 *  @return true if an active display, false if not
 */
bool
adl_is_valid_adlno(
      int iAdapterIndex,
      int iDisplayIndex,
      bool emit_error_msg)
{
   return (adl_get_display_by_adlno(iAdapterIndex, iDisplayIndex, emit_error_msg) != NULL);
}



//
// Wrapper ADL read and write functions
//

//
// Use ADL_Display_DDCBlockAccess_Get() to write and read DDC packets
//

/** Wrapper for ADL_Display_DDCBlockAccess_Get().
 *
 *  Used locally and in ADL tests
 */
Base_Status_ADL
call_ADL_Display_DDCBlockAccess_Get(
      int    iAdapterIndex,
      int    iDisplayIndex,
      int    iOption,
      int    iCommandIndex,
      int    iSendMsgLen,
      char * lpucSendMsgBuf,
      int *  iRecvMsgLen,
      char * lpucRcvMsgBuf)
{
   assert(module_initialized);
   int rc;

   if (adl_debug) {
      DBGMSG("iAdapterIndex=%d, iDisplayIndex=%d, iOption=%d, xxx=%d, iSendMsgLen=%d lpucSemdMsgBuf=%p,"             
             " *piRecvMsgLen=%d, lpucRcvMsgBuf=%p\n",       
             iAdapterIndex, iDisplayIndex, iOption, iCommandIndex, iSendMsgLen, lpucSendMsgBuf, *iRecvMsgLen, lpucRcvMsgBuf);
      if (lpucSendMsgBuf) {
         char * hs = hexstring(lpucSendMsgBuf, iSendMsgLen);
         DBGMSG("lpucSendMsgBuf -> %s  ", hs );
         free(hs);
      }
      if (lpucRcvMsgBuf) {
         char * hs = hexstring(lpucRcvMsgBuf, *iRecvMsgLen);
         DBGMSG("lpucRecvMsgBuf -> %s  ", hs  );
         free(hs);
      }
   }

   RECORD_IO_EVENT(
      IE_WRITE_READ,
      (
         rc = adl->ADL_Display_DDCBlockAccess_Get( iAdapterIndex, iDisplayIndex, iOption, iCommandIndex,
                             iSendMsgLen, lpucSendMsgBuf, iRecvMsgLen, lpucRcvMsgBuf)
      )
   );

   // rc = adl->ADL_Display_DDCBlockAccess_Get( iAdapterIndex, iDisplayIndex, iOption, xxx,
   //       iSendMsgLen, lpucSendMsgBuf, iRecvMsgLen, lpucRcvMsgBuf);
   if (adl_debug) {
      if (lpucRcvMsgBuf) {
         char * hs = hexstring(lpucRcvMsgBuf, *iRecvMsgLen);
         DBGMSG("lpucRecvMsgBuf -> %s  ", hs  );
         free(hs);
      }
      DBGMSG("Returning %d", rc);
   }
   return rc;

}


/** Writes a DDC packet to the specified ADL display
 *
 * @param  iAdapterIndex ADL adapter number
 * @param  iDisplayIndex ADL display number
 * @param  pSendMsgBuf   pointer to bytes to write
 * @param  sendMsgLen    number of bytes to write
 *
 * @return ADL status code
 *
 * @remark 10/2015: called locally and from ddc_packet_io.c
 */
Base_Status_ADL
adl_ddc_write_only(
      int     iAdapterIndex,
      int     iDisplayIndex,
      Byte *  pSendMsgBuf,
      int     sendMsgLen)
{
   assert(module_initialized);
   // char * s = hexstring(pSendMsgBuf, sendMsgLen);
   // printf("(%s) Starting. iAdapterIndex=%d, iDisplayIndex=%d, sendMsgLen=%d, pSendMsgBuf->%s\n",
   //        __func__, iAdapterIndex, iDisplayIndex, sendMsgLen, s);
   // free(s);

   int iRev = 0;

   int rc = call_ADL_Display_DDCBlockAccess_Get( iAdapterIndex, iDisplayIndex, 0, 0, sendMsgLen, pSendMsgBuf, &iRev, NULL);
   // note_io_event(IE_WRITE_ONLY, __func__);

   // DBGMSG("Returning %d. ", rc);
   return rc;
}

/** Reads a DDC packet from the specified ADL display
 *
 * @param  iAdapterIndex ADL adapter number
 * @param  iDisplayIndex ADL display number
 * @param  pRcvMsgBuf    pointer to buffer in which to return packet
 * @param  pRcvBytect    pointer to integer in which packet length returned
 *
 * @return ADL status code
 */
Base_Status_ADL adl_ddc_read_only(
      int     iAdapterIndex,
      int     iDisplayIndex,
      Byte *  pRcvMsgBuf,
      int *   pRcvBytect)
{
   assert(module_initialized);
   if (adl_debug) {
      DBGMSG("Starting. iAdapterIndex=%d, iDisplayIndex=%d, pRcvButect=%d", 
        iAdapterIndex, iDisplayIndex, *pRcvBytect);
   }

   Byte sendMsgBuf[] = {0x6f};

   int rc = call_ADL_Display_DDCBlockAccess_Get(
                    iAdapterIndex,
                    iDisplayIndex,
                    0,
                    0,
                    1,
                    sendMsgBuf,
                    pRcvBytect,
                    pRcvMsgBuf);

   if (adl_debug) {
      DBGMSG("Returning %d. ", rc);
      if (rc == 0) {
         char * s = hexstring(pRcvMsgBuf, *pRcvBytect);
         DBGMSG("*pRcvBytect=%d, pRevMsgBuf->%s   ",*pRcvBytect, s );
         free(s);
      }
   }
   return rc;
}


/**  Performs and DDC write followed by a DDC read.
 *
 *  @param  iAdapterIndex  ADL adapter number
 *  @param  iDisplayIndex  ADL display number
 *  @param  pSendMsgBuf    buffer containing bytes to send
 *  @param  sendMsgLen     number of bytes to send
 *  @param  pRcvMsgBuf     where to return bytes read
 *  @param  pRcvBytect     where to return number of bytes read
 *
 *  @return ADL status code
 *
 * @remark 10/2015: called from adl_services.c and ddc_packet_io.c
 */
Base_Status_ADL adl_ddc_write_read(
      int     iAdapterIndex,
      int     iDisplayIndex,
      Byte *  pSendMsgBuf,
      int     sendMsgLen,
      Byte *  pRcvMsgBuf,
      int *   pRcvBytect)
{
   assert(module_initialized);
   if (adl_debug) {
      char * s = hexstring(pSendMsgBuf, sendMsgLen);
      DBGMSG("Starting. iAdapterIndex=%d, iDisplayIndex=%d, sendMsgLen=%d, pSendMsgBuf->%s, *pRcvBytect=%d", 
        iAdapterIndex, iDisplayIndex, sendMsgLen, s, *pRcvBytect);
      free(s);
   }

   int rc;

   rc = adl_ddc_write_only(iAdapterIndex, iDisplayIndex, pSendMsgBuf, sendMsgLen);
   if (rc == 0) {
      sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, "after write");
      rc = adl_ddc_read_only(iAdapterIndex, iDisplayIndex, pRcvMsgBuf, pRcvBytect);
   }

   if (adl_debug) {
      DBGMSG("Returning %d. ", rc);
      if (rc == 0) {
         char * s = hexstring(pRcvMsgBuf, *pRcvBytect);
         DBGMSG("pRevMsgBuf->%s   ",s );
         free(s);
      }
   }
   return rc;
}


/**  Attempts to perform a DDC write followed by a DDC read using
 *   a single call to ADL_Display_DDCBlockAccess_Get(().
 *
 *   However, appears to just return the bytes written.
 *
 *   DO NOT USE
 *
 *  @param  iAdapterIndex  ADL adapter number
 *  @param  iDisplayIndex  ADL display number
 *  @param  pSendMsgBuf    buffer containing bytes to send
 *  @param  sendMsgLen     number of bytes to send
 *  @param  pRcvMsgBuf     where to return bytes read
 *  @param  pRcvBytect     where to return number of bytes read
 *
 *  @return ADL status code
 *
 * @remark 10/2015: only called in adl_aux_intf.c
 */
Base_Status_ADL adl_ddc_write_read_onecall(
      int     iAdapterIndex,
      int     iDisplayIndex,
      Byte *  pSendMsgBuf,
      int     sendMsgLen,
      Byte *  pRcvMsgBuf,
      int *   pRcvBytect)
{
   assert(module_initialized);
   if (adl_debug) {
      char * s = hexstring(pSendMsgBuf, sendMsgLen);
      DBGMSG("Starting. iAdapterIndex=%d, iDisplayIndex=%d, sendMsgLen=%d, pSendMsgBuf->%s, *pRcvBytect=%d", 
        iAdapterIndex, iDisplayIndex, sendMsgLen, s, *pRcvBytect);
      free(s);
   }

   // try to write and read in one call
   int rc = call_ADL_Display_DDCBlockAccess_Get(
                     iAdapterIndex,
                     iDisplayIndex,
                     0,
                     0,
                     sendMsgLen,
                     pSendMsgBuf,
                     pRcvBytect,
                     pRcvMsgBuf);
   if (adl_debug) {
      DBGMSG("Returning %d. ", rc);
      if (rc == 0) {
         char * s = hexstring(pRcvMsgBuf, *pRcvBytect);
         DBGMSG("pRevMsgBuf->%s   ",s );
         free(s);
      }
   }
   return rc;
}


