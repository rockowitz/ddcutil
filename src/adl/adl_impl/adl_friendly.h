/*
 * adl_friendly.h
 *
 *  Created on: Jul 27, 2014
 *      Author: rock
 *
 *  Type definitions, function declarations, etc that should be private to adl_intf.c,
 *  but need to be visible to other ADL related files, particularly tests.
 */

#ifndef ADL_FRIENDLY_H_
#define ADL_FRIENDLY_H_

#include <base/util.h>

#include "adl/adl_impl/adl_sdk_includes.h"
#include "adl/adl_impl/adl_intf.h"


#define MAX_ACTIVE_DISPLAYS 16

typedef
struct {
    // bool initialized;

    void *hDLL;

    int ( *ADL_Main_Control_Create )   (ADL_MAIN_MALLOC_CALLBACK, int iEnumConnectedAdapters );
    int ( *ADL_Main_Control_Destroy )  ();

    int (*ADL_Adapter_NumberOfAdapters_Get)(int *lpNumAdapters);
    int (*ADL_Adapter_AdapterInfo_Get)(LPAdapterInfo lpInfo, int iInputSize);
    int (*ADL_Display_NumberOfDisplays_Get)(int iAdapterIndex, int *lpNumDisplays);
    int (*ADL_Display_DisplayInfo_Get)(int  iAdapterIndex, int *lpNumDisplays, ADLDisplayInfo **lppInfo, int iForceDetect);

    int (  *ADL_Display_ColorCaps_Get   )  (int, int, int *, int *);
    int (  *ADL_Display_Color_Get ) ( int, int, int, int *, int *, int *, int *, int * );
    int (  *ADL_Display_Color_Set ) ( int, int, int, int );
    int ( *ADL2_Display_ColorCaps_Get   )  (ADL_CONTEXT_HANDLE context, int, int, int *, int *);
    int ( *ADL2_Display_Color_Get ) (ADL_CONTEXT_HANDLE context,  int, int, int, int *, int *, int *, int *, int * );
    int ( *ADL2_Display_Color_Set ) (ADL_CONTEXT_HANDLE context,  int, int, int, int );

    // I2C, DDC, and EDID APIs
    int ( *ADL2_Display_WriteAndReadI2CRev_Get ) (ADL_CONTEXT_HANDLE context, int iAdapterIndex, int * lpMajor, int * lpMinor);
    int ( * ADL_Display_WriteAndReadI2CRev_Get ) (int iAdapterIndex, int * lpMajor, int * lpMinor);
    int ( *ADL2_Display_WriteAndReadI2C )  (ADL_CONTEXT_HANDLE context, int iAdapterIndex, ADLI2C * pii2C);
    int ( * ADL_Display_WriteAndReadI2C )  (int iAdapterIndex, ADLI2C * pii2C);
    int ( *ADL2_Display_DDCBlockAccess_Get) (ADL_CONTEXT_HANDLE context,
                                             int                iAdapterIndex,
                                             int                iDisplayIndex,
                                             int                iOption,
                                             int                iCommandIndex,
                                             int                iSendMsgLen,
                                             char *             lpucSendMsgBuf,
                                             int *              lpulRecvMsgLen,
                                             char *             lpucRecvMsgBuf
                                            );
    int ( * ADL_Display_DDCBlockAccess_Get) (
                                             int                iAdapterIndex,
                                             int                iDisplayIndex,
                                             int                iOption,
                                             int                iCommandIndex,
                                             int                iSendMsgLen,
                                             char *             lpucSendMsgBuf,
                                             int *              lpulRecvMsgLen,
                                             char *             lpucRecvMsgBuf
                                                    );
    int ( *ADL2_Display_DDCInfo_Get  ) (ADL_CONTEXT_HANDLE context, int iAdapterIndex, int iDisplayIndex, ADLDDCInfo * lpinfo);
    int ( * ADL_Display_DDCInfo_Get  ) (                            int iAdapterIndex, int iDisplayIndex, ADLDDCInfo * lpinfo);
    int ( *ADL2_Display_DDCInfo2_Get ) (ADL_CONTEXT_HANDLE context, int iAdapterIndex, int iDisplayIndex, ADLDDCInfo2 * lpinfo);
    int ( * ADL_Display_DDCInfo2_Get ) (                            int iAdapterIndex, int iDisplayIndex, ADLDDCInfo2 * lpinfo);
    int ( *ADL2_Display_EdidData_Get ) (ADL_CONTEXT_HANDLE context, int iAdapterIndex, int iDisplayIndex, ADLDisplayEDIDData* lpEDIDData);
    int ( * ADL_Display_EdidData_Get ) (                            int iAdapterIndex, int iDisplayIndex, ADLDisplayEDIDData* lpEDIDData);

    // Linux only APIs
    int ( *ADL2_Adapter_XScreenInfo_Get       ) (ADL_CONTEXT_HANDLE context,
                                                 XScreenInfo *      lpXScreeninfo,
                                                 int inputSize);
    int ( * ADL_Adapter_XScreenInfo_Get       ) (
                                                 XScreenInfo *      lpXScreeninfo,
                                                 int                inputSize);
    int ( *ADL2_Display_XrandrDisplayName_Get ) (ADL_CONTEXT_HANDLE context,
                                                 int                iAdapterIndex,
                                                 int                iDisplayIndex,
                                                 char *             lpXrandrDisplayName,
                                                 int                iBuffSize);
    int ( * ADL_Display_XrandrDisplayName_Get ) (
                                                 int                iAdapterIndex,
                                                 int                iDisplayIndex,
                                                 char *             lpXrandrDisplayName,
                                                 int                iBuffSize);
} Adl_Procs;


// defined in adl_intf.c:
extern bool            adl_debug;
extern Adl_Procs*      adl;
extern int             active_display_ct;
extern ADL_Display_Rec active_displays[MAX_ACTIVE_DISPLAYS];

int call_ADL_Display_DDCBlockAccess_Get(
      int    iAdapterIndex,
      int    iDisplayIndex,
      int    iOption,
      int    xxx,
      int    iSendMsgLen,
      char * lpucSendMsgBuf,
      int *  iRecvMsgLen,
      char * lpucRcvMsgBuf);

#endif /* ADL_FRIENDLY_H_ */
