/* adl_report.c
 *
 * Report on data structures in ADL SDK
 *
 * <copyright>
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
 * Reports on data structures in ADL SDK.
 *
 * Used only for development and debugging.
 */

/** \cond */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/** \endcond */

#include "util/edid.h"
#include "util/string_util.h"

#include "base/core.h"
#include "adl/adl_impl/adl_sdk_includes.h"

#include "adl/adl_impl/adl_report.h"


#ifdef REFERENCE
/// \defgroup define_edid_flags Values for ulDDCInfoFlag
/// defines for ulDDCInfoFlag EDID flag
// @{
#define ADL_DISPLAYDDCINFOEX_FLAG_PROJECTORDEVICE       (1 << 0)
#define ADL_DISPLAYDDCINFOEX_FLAG_EDIDEXTENSION         (1 << 1)
#define ADL_DISPLAYDDCINFOEX_FLAG_DIGITALDEVICE         (1 << 2)
#define ADL_DISPLAYDDCINFOEX_FLAG_HDMIAUDIODEVICE       (1 << 3)
#define ADL_DISPLAYDDCINFOEX_FLAG_SUPPORTS_AI           (1 << 4)
#define ADL_DISPLAYDDCINFOEX_FLAG_SUPPORT_xvYCC601      (1 << 5)
#define ADL_DISPLAYDDCINFOEX_FLAG_SUPPORT_xvYCC709      (1 << 6)
// @}
#endif

#ifdef REFERENCE

///////////////////////////////////////////////////////////////////////////
// ADL_DISPLAY_DISPLAYINFO_ Definitions
// for ADLDisplayInfo.iDisplayInfoMask and ADLDisplayInfo.iDisplayInfoValue
// (bit-vector)
///////////////////////////////////////////////////////////////////////////
/// \defgroup define_displayinfomask Display Info Mask Values
// @{
#define ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED         0x00000001
#define ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED            0x00000002
#define ADL_DISPLAY_DISPLAYINFO_NONLOCAL              0x00000004
#define ADL_DISPLAY_DISPLAYINFO_FORCIBLESUPPORTED        0x00000008
#define ADL_DISPLAY_DISPLAYINFO_GENLOCKSUPPORTED         0x00000010
#define ADL_DISPLAY_DISPLAYINFO_MULTIVPU_SUPPORTED       0x00000020
#define ADL_DISPLAY_DISPLAYINFO_LDA_DISPLAY              0x00000040
#define ADL_DISPLAY_DISPLAYINFO_MODETIMING_OVERRIDESSUPPORTED        0x00000080

#define ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_SINGLE        0x00000100
#define ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_CLONE         0x00000200

/// Legacy support for XP
#define ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_2VSTRETCH     0x00000400
#define ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_2HSTRETCH     0x00000800
#define ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_EXTENDED      0x00001000

/// More support manners
#define ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_NSTRETCH1GPU  0x00010000
#define ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_NSTRETCHNGPU  0x00020000
#define ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_RESERVED2     0x00040000
#define ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_RESERVED3     0x00080000

/// Projector display type
#define ADL_DISPLAY_DISPLAYINFO_SHOWTYPE_PROJECTOR          0x00100000


#endif

#define FLAG_INFO(name,info) {#name, info, name}

// TODO: If code is cleaned up, replace with a Value_Name_Title table.

Flag_Info all_flags[] = {
   // ulDDCInfoFlag
   {"ADL_DISPLAYDDCINFOEX_FLAG_PROJECTORDEVICE",  NULL, ADL_DISPLAYDDCINFOEX_FLAG_PROJECTORDEVICE   },
   {"ADL_DISPLAYDDCINFOEX_FLAG_EDIDEXTENSION",    NULL, ADL_DISPLAYDDCINFOEX_FLAG_PROJECTORDEVICE   },
   {"ADL_DISPLAYDDCINFOEX_FLAG_DIGITALDEVICE",    NULL, ADL_DISPLAYDDCINFOEX_FLAG_DIGITALDEVICE  },
   {"ADL_DISPLAYDDCINFOEX_FLAG_HDMIAUDIODEVICE",  NULL, ADL_DISPLAYDDCINFOEX_FLAG_HDMIAUDIODEVICE   },
   {"ADL_DISPLAYDDCINFOEX_FLAG_SUPPORTS_AI",      NULL, ADL_DISPLAYDDCINFOEX_FLAG_SUPPORTS_AI   },
   {"ADL_DISPLAYDDCINFOEX_FLAG_SUPPORT_xvYCC601", NULL, ADL_DISPLAYDDCINFOEX_FLAG_SUPPORT_xvYCC601  },
   {"ADL_DISPLAYDDCINFOEX_FLAG_SUPPORT_xvYCC709", NULL, ADL_DISPLAYDDCINFOEX_FLAG_SUPPORT_xvYCC709  },

   // ADLDisplayInfo.iDisplayInfoMask, .iDisplayInfoValue
   // @{
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED               , NULL),
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED                  , NULL),
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_NONLOCAL                       , NULL),
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_FORCIBLESUPPORTED              , NULL),
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_GENLOCKSUPPORTED               , NULL),
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_MULTIVPU_SUPPORTED             , NULL),
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_LDA_DISPLAY                    , NULL),
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_MODETIMING_OVERRIDESSUPPORTED  , NULL),
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_SINGLE        , NULL),
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_CLONE         , NULL),

   /// Legacy support for XP
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_2VSTRETCH     , NULL),
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_2HSTRETCH     , NULL),
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_EXTENDED      , NULL),

   /// More support manners
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_NSTRETCH1GPU  , NULL),
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_NSTRETCHNGPU  , NULL),
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_RESERVED2     , NULL),
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_RESERVED3     , NULL),

   /// Projector display type
   FLAG_INFO(ADL_DISPLAY_DISPLAYINFO_SHOWTYPE_PROJECTOR             , NULL),
};
#define all_flags_ct  ( sizeof(all_flags)/sizeof(Flag_Info) )

Flag_Dictionary all_flags_dict = { all_flags_ct, all_flags };


char * ddcInfoFlagNames[] = {
      "ADL_DISPLAYDDCINFOEX_FLAG_PROJECTORDEVICE",
      "ADL_DISPLAYDDCINFOEX_FLAG_EDIDEXTENSION",
      "ADL_DISPLAYDDCINFOEX_FLAG_DIGITALDEVICE",
      "ADL_DISPLAYDDCINFOEX_FLAG_HDMIAUDIODEVICE",
      "ADL_DISPLAYDDCINFOEX_FLAG_SUPPORTS_AI",
      "ADL_DISPLAYDDCINFOEX_FLAG_SUPPORT_xvYCC601",
      "ADL_DISPLAYDDCINFOEX_FLAG_SUPPORT_xvYCC709"
};
Flag_Name_Set ddcInfoFlagNameSet = { sizeof(ddcInfoFlagNames)/sizeof(char *), ddcInfoFlagNames };

char * displayInfoValueNames[] = {
      "ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED",
      "ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED",
      "ADL_DISPLAY_DISPLAYINFO_NONLOCAL",
      "ADL_DISPLAY_DISPLAYINFO_FORCIBLESUPPORTED",
      "ADL_DISPLAY_DISPLAYINFO_GENLOCKSUPPORTED",
      "ADL_DISPLAY_DISPLAYINFO_MULTIVPU_SUPPORTED",
      "ADL_DISPLAY_DISPLAYINFO_LDA_DISPLAY",
      "ADL_DISPLAY_DISPLAYINFO_MODETIMING_OVERRIDESSUPPORTED",
      "ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_SINGLE",
      "ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_CLONE",

         /// Legacy support for XP
      "ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_2VSTRETCH",
      "ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_2HSTRETCH",
      "ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_EXTENDED",

         /// More support manners
      "ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_NSTRETCH1GPU",
      "ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_NSTRETCHNGPU",
      "ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_RESERVED2",
      "ADL_DISPLAY_DISPLAYINFO_MANNER_SUPPORTED_RESERVED3",

         /// Projector display type
      "ADL_DISPLAY_DISPLAYINFO_SHOWTYPE_PROJECTOR",
};
Flag_Name_Set displayInfoFlagNameSet = {sizeof(displayInfoValueNames)/sizeof(char*), displayInfoValueNames};




#ifdef REFERENCE
typedef struct AdapterInfo
{
/// \ALL_STRUCT_MEM

/// Size of the structure.
    int iSize;
/// The ADL index handle. One GPU may be associated with one or two index handles
    int iAdapterIndex;
/// The unique device ID associated with this adapter.
    char strUDID[ADL_MAX_PATH];
/// The BUS number associated with this adapter.
    int iBusNumber;
/// The driver number associated with this adapter.
    int iDeviceNumber;
/// The function number.
    int iFunctionNumber;
/// The vendor ID associated with this adapter.
    int iVendorID;
/// Adapter name.
    char strAdapterName[ADL_MAX_PATH];
/// Display name. For example, "\\Display0" for Windows or ":0:0" for Linux.
    char strDisplayName[ADL_MAX_PATH];
/// Present or not; 1 if present and 0 if not present.It the logical adapter is present, the display name such as \\.\Display1 can be found from OS
   int iPresent;
// @}

#if defined (_WIN32) || defined (_WIN64)
/// \WIN_STRUCT_MEM

/// Exist or not; 1 is exist and 0 is not present.
    int iExist;
/// Driver registry path.
    char strDriverPath[ADL_MAX_PATH];
/// Driver registry path Ext for.
    char strDriverPathExt[ADL_MAX_PATH];
/// PNP string from Windows.
    char strPNPString[ADL_MAX_PATH];
/// It is generated from EnumDisplayDevices.
    int iOSDisplayIndex;
// @}
#endif /* (_WIN32) || (_WIN64) */

#if defined (LINUX)
/// \LNX_STRUCT_MEM

/// Internal X screen number from GPUMapInfo (DEPRICATED use XScreenInfo)
    int iXScreenNum;
/// Internal driver index from GPUMapInfo
    int iDrvIndex;
/// \deprecated Internal x config file screen identifier name. Use XScreenInfo instead.
    char strXScreenConfigName[ADL_MAX_PATH];

// @}
#endif /* (LINUX) */
} AdapterInfo, *LPAdapterInfo;

#endif




/** */
void report_adl_AdapterInfo(AdapterInfo * pAdapterInfo, int depth) {
   rpt_structure_loc("AdapterInfo", pAdapterInfo, depth);
   printf("     iSize (size of structure): %d\n",                         pAdapterInfo->iSize);
   printf("     iAdapterIndex (ADL index handle): %d\n",                  pAdapterInfo->iAdapterIndex);
   printf("     strUUID (UUID for this adapter): %s\n",                   pAdapterInfo->strUDID);
   printf("     iBusNumber (bus number for this adapter):  %d\n",         pAdapterInfo->iBusNumber);
   printf("     iDeviceNumber (device number for this adapter): %d\n",    pAdapterInfo->iDeviceNumber);
   printf("     iFunctionNumber (function number):  %d\n",                pAdapterInfo->iFunctionNumber);
   printf("     iVendorID (vendor ID):  %d\n",                            pAdapterInfo->iVendorID);
   printf("     strAdapterName (adapter name): %s\n",                     pAdapterInfo->strAdapterName);
   printf("     strDisplayName (display name): %s\n",                     pAdapterInfo->strDisplayName);
   printf("     iPresent (is logical adapter present) %d\n",              pAdapterInfo->iPresent);
   printf("     iXScreenNum (deprecated, use XScreenInfo): %d\n",         pAdapterInfo->iXScreenNum);
   printf("     iDrvIndex (internal driver index from GPUMapInfo): %d\n", pAdapterInfo->iDrvIndex);
   printf("     strXScreenConfigName (deprecated, use XSCreenInfo): %s\n",
                                                                    pAdapterInfo->strXScreenConfigName);
}


#ifdef REFERENCE


/////////////////////////////////////////////////////////////////////////////////////////////
///\brief Structure containing information about the display device.
///
/// This structure is used to store display device information
/// such as display index, type, name, connection status, mapped adapter and controller indexes,
/// whether or not multiple VPUs are supported, local display connections or not (through Lasso), etc.
/// This information can be returned to the user. Alternatively, it can be used to access various driver calls to set
/// or fetch various display device related settings upon the user's request.
/// \nosubgrouping
////////////////////////////////////////////////////////////////////////////////////////////
typedef struct ADLDisplayID
{
/// The logical display index belonging to this adapter.
   int iDisplayLogicalIndex;

///\brief The physical display index.
/// For example, display index 2 from adapter 2 can be used by current adapter 1.\n
/// So current adapter may enumerate this adapter as logical display 7 but the physical display
/// index is still 2.
   int iDisplayPhysicalIndex;

/// The persistent logical adapter index for the display.
   int iDisplayLogicalAdapterIndex;

///\brief The persistent physical adapter index for the display.
/// It can be the current adapter or a non-local adapter. \n
/// If this adapter index is different than the current adapter,
/// the Display Non Local flag is set inside DisplayInfoValue.
    int iDisplayPhysicalAdapterIndex;
} ADLDisplayID, *LPADLDisplayID;

#endif

/** */
void report_adl_ADLDisplayID(ADLDisplayID * pADLDisplayID, int depth) {
   rpt_structure_loc("ADLDisplayID", pADLDisplayID, depth);
   int d = depth + 1;
   rpt_int("iDisplayLogicalIndex",        "logical display index for this adapter", pADLDisplayID->iDisplayLogicalIndex,        d);
   rpt_int("iDisplayPhysicalIndex",       "physical display index",                 pADLDisplayID->iDisplayPhysicalIndex,       d);
   rpt_int("iDisplayLogicalAdapterIndex", "persistent logical adapter index",       pADLDisplayID->iDisplayLogicalAdapterIndex, d);
   rpt_int("iDisplayPhysicalAdapterIndex", NULL,                                    pADLDisplayID->iDisplayPhysicalAdapterIndex,d);
}



#ifdef REFERENCE
/////////////////////////////////////////////////////////////////////////////////////////////
///\brief Structure containing information about the display device.
///
/// This structure is used to store various information about the display device.  This
/// information can be returned to the user, or used to access various driver calls to set
/// or fetch various display-device-related settings upon the user's request
/// \nosubgrouping
////////////////////////////////////////////////////////////////////////////////////////////
typedef struct ADLDisplayInfo
{
/// The DisplayID structure
   ADLDisplayID displayID;

///\deprecated The controller index to which the display is mapped.\n Will not be used in the future\n
   int  iDisplayControllerIndex;

/// The display's EDID name.
   char strDisplayName[ADL_MAX_PATH];

/// The display's manufacturer name.
   char strDisplayManufacturerName[ADL_MAX_PATH];

/// The Display type. For example: CRT, TV, CV, DFP.
   int  iDisplayType;

/// The display output type. For example: HDMI, SVIDEO, COMPONMNET VIDEO.
   int  iDisplayOutputType;

/// The connector type for the device.
   int  iDisplayConnector;

///\brief The bit mask identifies the number of bits ADLDisplayInfo is currently using. \n
/// It will be the sum all the bit definitions in ADL_DISPLAY_DISPLAYINFO_xxx.
   int  iDisplayInfoMask;

/// The bit mask identifies the display status. \ref define_displayinfomask
   int  iDisplayInfoValue;
} ADLDisplayInfo, *LPADLDisplayInfo;

#endif

#ifdef reference
/// \defgroup define_display_type Display Type
/// Define Monitor/CRT display type
// @{
/// Define Monitor display type
#define ADL_DT_MONITOR              0
/// Define TV display type
#define ADL_DT_TELEVISION                 1
/// Define LCD display type
#define ADL_DT_LCD_PANEL                     2
/// Define DFP display type
#define ADL_DT_DIGITAL_FLAT_PANEL      3
/// Define Componment Video display type
#define ADL_DT_COMPONENT_VIDEO            4
/// Define Projector display type
#define ADL_DT_PROJECTOR                    5
// @}


/// \defgroup define_display_connection_type Display Connection Type
// @{
/// Define unknown display output type
#define ADL_DOT_UNKNOWN          0
/// Define composite display output type
#define ADL_DOT_COMPOSITE        1
/// Define SVideo display output type
#define ADL_DOT_SVIDEO           2
/// Define analog display output type
#define ADL_DOT_ANALOG           3
/// Define digital display output type
#define ADL_DOT_DIGITAL          4
// @}


/// \defgroup define_displayinfo_connector Display Connector Type
/// defines for ADLDisplayInfo.iDisplayConnector
// @{
#define ADL_DISPLAY_CONTYPE_UNKNOWN                 0
#define ADL_DISPLAY_CONTYPE_VGA                     1
#define ADL_DISPLAY_CONTYPE_DVI_D                   2
#define ADL_DISPLAY_CONTYPE_DVI_I                   3
#define ADL_DISPLAY_CONTYPE_ATICVDONGLE_NTSC        4
#define ADL_DISPLAY_CONTYPE_ATICVDONGLE_JPN         5
#define ADL_DISPLAY_CONTYPE_ATICVDONGLE_NONI2C_JPN  6
#define ADL_DISPLAY_CONTYPE_ATICVDONGLE_NONI2C_NTSC 7
#define ADL_DISPLAY_CONTYPE_PROPRIETARY            8
#define ADL_DISPLAY_CONTYPE_HDMI_TYPE_A             10
#define ADL_DISPLAY_CONTYPE_HDMI_TYPE_B             11
#define ADL_DISPLAY_CONTYPE_SVIDEO                 12
#define ADL_DISPLAY_CONTYPE_COMPOSITE               13
#define ADL_DISPLAY_CONTYPE_RCA_3COMPONENT          14
#define ADL_DISPLAY_CONTYPE_DISPLAYPORT             15
#define ADL_DISPLAY_CONTYPE_EDP                     16
#define ADL_DISPLAY_CONTYPE_WIRELESSDISPLAY         17
// @}


#endif


static char * displayTypeNames[] = {
      "ADL_DT_MONITOR",
      "ADL_DT_TELEVISION",
      "ADL_DT_LCD_PANEL",
      "ADL_DT_DIGITAL_FLAT_PANEL",
      "ADL_DT_COMPONENT_VIDEO",
      "ADL_DT_PROJECTOR"
};
static int displayTypeNameCt = sizeof(displayTypeNames)/sizeof(char *);

static char * displayOutputTypeNames[] = {
      "ADL_DOT_UNKNOWN",
      "ADL_DOT_COMPOSITY",
      "ADL_DOT_SVIDEO",
      "ADL_DOT_ANALOG",
      "ADL_DOT_DIGITAL"
};
static int displayOutputTypeNameCt = sizeof(displayOutputTypeNames)/sizeof(char *);

static char * displayConnectorTypeNames[] = {
      "ADL_DISPLAY_CONTYPE_UNKNOWN",                 //  0
      "ADL_DISPLAY_CONTYPE_VGA",                     //  1
      "ADL_DISPLAY_CONTYPE_DVI_D",                   //  2
      "ADL_DISPLAY_CONTYPE_DVI_I",                   //  3
      "ADL_DISPLAY_CONTYPE_ATICVDONGLE_NTSC",        //  4
      "ADL_DISPLAY_CONTYPE_ATICVDONGLE_JPN",         //  5
      "ADL_DISPLAY_CONTYPE_ATICVDONGLE_NONI2C_JPN",  //  6
      "ADL_DISPLAY_CONTYPE_ATICVDONGLE_NONI2C_NTSC", //  7
      "ADL_DISPLAY_CONTYPE_PROPRIETARY",             //  8
      "INVALID CODE",                                //  9 - not defined
      "ADL_DISPLAY_CONTYPE_HDMI_TYPE_A",             // 10
      "ADL_DISPLAY_CONTYPE_HDMI_TYPE_B",             // 11
      "ADL_DISPLAY_CONTYPE_SVIDEO",                  // 12
      "ADL_DISPLAY_CONTYPE_COMPOSITE",               // 13
      "ADL_DISPLAY_CONTYPE_RCA_3COMPONENT",          // 14
      "ADL_DISPLAY_CONTYPE_DISPLAYPORT",             // 15
      "ADL_DISPLAY_CONTYPE_EDP",                     // 16
      "ADL_DISPLAY_CONTYPE_WIRELESSDISPLAY"          // 17
   };
static int displayConnectorTypeNamesCt = sizeof(displayConnectorTypeNames) / sizeof(char *);


/** */
char * displayTypeName(int iDisplayType) {
   assert(0 <= iDisplayType && iDisplayType < displayTypeNameCt);
   return displayTypeNames[iDisplayType];
}

/** */
char * displayOutputTypeName(int iDisplayOutputType) {
   assert(0 <= iDisplayOutputType && iDisplayOutputType < displayOutputTypeNameCt);
   return displayOutputTypeNames[iDisplayOutputType];

}

/** */
char * displayConnectorTypeName(int iDisplayConnector) {
   assert(0 <= iDisplayConnector && iDisplayConnector < displayConnectorTypeNamesCt);
   return displayConnectorTypeNames[iDisplayConnector];
}

/** */
void report_adl_ADLDisplayInfo(ADLDisplayInfo * pADLDisplayInfo, int depth) {
   rpt_structure_loc("ADLDisplayInfo", pADLDisplayInfo, depth);
   if (pADLDisplayInfo) {
      rpt_title("ADLDisplayID:", depth+1);
      report_adl_ADLDisplayID(&pADLDisplayInfo->displayID,depth+2);
      int d = depth+1;
      rpt_int("iDisplayControllerIndex",    "deprecated",                pADLDisplayInfo->iDisplayControllerIndex,    d);
      rpt_str("strDisplayName",             "EDID name",                 pADLDisplayInfo->strDisplayName,             d);
      rpt_str("strDisplayManufacturerName", "display mfg name",          pADLDisplayInfo->strDisplayManufacturerName, d);
      rpt_mapped_int("iDisplayType",              "e.g. CRT, DFP",             pADLDisplayInfo->iDisplayType,       displayTypeName,        d);
      rpt_mapped_int("iDisplayOutputType",        "e.g. HDMI",                 pADLDisplayInfo->iDisplayOutputType, displayOutputTypeName,        d);
      rpt_mapped_int("iDisplayConnector",         "connector type",            pADLDisplayInfo->iDisplayConnector,  displayConnectorTypeName,       d);
      rpt_int_as_hex("iDisplayInfoMask",          "bits ADLDisplayInfo using", pADLDisplayInfo->iDisplayInfoMask,           d);
      rpt_ifval2("iDisplayInfoMask",         "bits ADLDisplayInfo using", pADLDisplayInfo->iDisplayInfoMask, &displayInfoFlagNameSet, &all_flags_dict,         d);
      rpt_int_as_hex("iDisplayInfoValue",         "display status",            pADLDisplayInfo->iDisplayInfoValue,          d);
      rpt_ifval2("iDisplayInfoValue",        "display status",            pADLDisplayInfo->iDisplayInfoValue,  &displayInfoFlagNameSet,  &all_flags_dict,       d);
   }
}

#ifdef REFERENCE

/////////////////////////////////////////////////////////////////////////////////////////////
///\brief Structure containing information about EDID data.
///
/// This structure is used to store the information about EDID data for the adapter.
/// This structure is used by the ADL_Display_EdidData_Get() and ADL_Display_EdidData_Set() functions.
/// \nosubgrouping
////////////////////////////////////////////////////////////////////////////////////////////
typedef struct ADLDisplayEDIDData
{
/// Size of the structure
  int iSize;
/// Set to 0
  int iFlag;
  /// Size of cEDIDData. Set by ADL_Display_EdidData_Get() upon return
  int iEDIDSize;
/// 0, 1 or 2. If set to 3 or above an error ADL_ERR_INVALID_PARAM is generated
  int iBlockIndex;
/// EDID data
  char cEDIDData[ADL_MAX_EDIDDATA_SIZE];
/// Reserved
  int iReserved[4];
}ADLDisplayEDIDData;
#endif

/** */
void report_adl_ADLDisplayEDIDData(ADLDisplayEDIDData * pEDIDData, int depth) {
   rpt_structure_loc("ADLDisplayEDIDData", pEDIDData, depth);
   if (pEDIDData) {
      int d = depth+1;
      rpt_int( "iSize",       "size of structure", pEDIDData->iSize,       d);
      rpt_int_as_hex("iFlag",       "",                  pEDIDData->iFlag,       d);
      rpt_int( "iEDIDSize",   "size of cEDIDData", pEDIDData->iEDIDSize,   d);
      rpt_int( "iBlockIndex", "0,1,2",             pEDIDData->iBlockIndex, d);
      printf("%*scEDIDData:\n", rpt_get_indent(d), "");
      hex_dump((unsigned char *) pEDIDData->cEDIDData, ADL_MAX_EDIDDATA_SIZE);
   }
}

#ifdef REFERENCE

/////////////////////////////////////////////////////////////////////////////////////////////
///\brief Structure containing DDC information.
///
/// This structure is used to store various DDC information that can be returned to the user.
/// Note that all fields of type int are actually defined as unsigned int types within the driver.
/// \nosubgrouping
////////////////////////////////////////////////////////////////////////////////////////////
typedef struct ADLDDCInfo2
{
/// Size of the structure
    int  ulSize;
/// Indicates whether the attached display supports DDC. If this field is zero on return, no other DDC
/// information fields will be used.
    int  ulSupportsDDC;
/// Returns the manufacturer ID of the display device. Should be zeroed if this information is not available.
    int  ulManufacturerID;
/// Returns the product ID of the display device. Should be zeroed if this information is not available.
    int  ulProductID;
/// Returns the name of the display device. Should be zeroed if this information is not available.
    char cDisplayName[ADL_MAX_DISPLAY_NAME];
/// Returns the maximum Horizontal supported resolution. Should be zeroed if this information is not available.
    int  ulMaxHResolution;
/// Returns the maximum Vertical supported resolution. Should be zeroed if this information is not available.
    int  ulMaxVResolution;
/// Returns the maximum supported refresh rate. Should be zeroed if this information is not available.
    int  ulMaxRefresh;
/// Returns the display device preferred timing mode's horizontal resolution.
    int  ulPTMCx;
/// Returns the display device preferred timing mode's vertical resolution.
    int  ulPTMCy;
/// Returns the display device preferred timing mode's refresh rate.
    int  ulPTMRefreshRate;
/// Return EDID flags.
    int  ulDDCInfoFlag;
// Returns 1 if the display supported packed pixel, 0 otherwise
    int bPackedPixelSupported;
// Returns the Pixel formats the display supports \ref define_ddcinfo_pixelformats
    int iPanelPixelFormat;
/// Return EDID serial ID.
    int  ulSerialID;
// Reserved for future use
    int iReserved[26];
} ADLDDCInfo2, *LPADLDDCInfo2;

#endif

/** */
void report_adl_ADLDDCInfo2( ADLDDCInfo2 * pStruct, bool verbose, int depth) {
   rpt_structure_loc("ADLDDCInfo2", pStruct, depth);
   int d = depth+1;
   rpt_int( "ulSize",   "size of structure", pStruct->ulSize, d);
   rpt_int( "ulSupportsDDC", "does display support DDC",        pStruct->ulSupportsDDC,   d);
   if (pStruct->ulSupportsDDC) {
      rpt_int( "ulManufacturerID", "manufacturer id",           pStruct->ulManufacturerID, d);
      rpt_int_as_hex("ulManufacturerID", "manufacturer id",           pStruct->ulManufacturerID, d);
      ushort us = pStruct->ulManufacturerID;
      char parsedMfgId[4];
      parse_mfg_id_in_buffer((Byte*)&us, parsedMfgId, sizeof(parsedMfgId));
      rpt_str( "ulManufacturerID", "manufacturer id",           parsedMfgId,               d);
      rpt_int( "ulProductID",      "product id",                pStruct->ulProductID,      d);
      rpt_str( "cDisplayName",     "name of display device",    pStruct->cDisplayName,     d);
      if (verbose) {
      rpt_int( "ulMaxHResolution", "max horizontal resolution", pStruct->ulMaxHResolution, d);
      rpt_int( "ulMaxVResolution", "max vertical resolution",   pStruct->ulMaxVResolution, d);
      rpt_int( "ulMaxRefresh",     "max refresh rate", pStruct->ulMaxRefresh, d);
      rpt_int( "ulPTMCx",          "preferred horizontal res",  pStruct->ulPTMCx, d);
      rpt_int( "ulPTMCy",          "preferred vertical res",    pStruct->ulPTMCy, d);
      rpt_int( "ulPTMRefreshRate", "preferred refresh rate",    pStruct->ulPTMRefreshRate, d);
      }
      rpt_int_as_hex("ulDDCInfoFlag",    "EDID flags",                pStruct->ulDDCInfoFlag, d);
      rpt_ifval2("ulDDCInfoFlag",    "EDID flags",  pStruct->ulDDCInfoFlag, &ddcInfoFlagNameSet, &all_flags_dict, d);
      if (verbose) {
      rpt_int("bPackedPixelSupported", "supports packed pixel?", pStruct->bPackedPixelSupported, d);
      rpt_int_as_hex("iPanelPixelFormat",  "pixel formats supported",  pStruct->iPanelPixelFormat, d);
      }
      rpt_int( "ulSerialID",         "EDID serial ID",          pStruct->ulSerialID, d);
   }


}

