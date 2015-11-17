typedef int ( *ADL_MAIN_CONTROL_CREATE )(ADL_MAIN_MALLOC_CALLBACK, int );
typedef int ( *ADL_MAIN_CONTROL_DESTROY )();
typedef int ( *ADL_ADAPTER_NUMBEROFADAPTERS_GET ) ( int* );
typedef int ( *ADL_ADAPTER_ADAPTERINFO_GET ) ( LPAdapterInfo, int );
typedef int ( *ADL_DISPLAY_DISPLAYINFO_GET ) ( int, int *, ADLDisplayInfo **, int );
typedef int ( *ADL_DISPLAY_DDCBLOCKACCESSGET ) ( int iAdapterIndex, int iDisplayIndex, int iOption, int iCommandIndex,int iSendMsgLen, char *lpucSendMsgBuf, int *lpulRecvMsgLen, char *lpucRecvMsgBuf);
typedef int ( *ADL_DISPLAY_EDIDDATA_GET ) ( int iAdapterIndex, int iDisplayIndex, ADLDisplayEDIDData *lpEDIDData);


typedef struct _ADLPROCS
{
    HMODULE hModule;
	ADL_MAIN_CONTROL_CREATE						ADL_Main_Control_Create;
	ADL_MAIN_CONTROL_DESTROY						ADL_Main_Control_Destroy;
	ADL_ADAPTER_NUMBEROFADAPTERS_GET	ADL_Adapter_NumberOfAdapters_Get;
	ADL_ADAPTER_ADAPTERINFO_GET				ADL_Adapter_AdapterInfo_Get;
    ADL_DISPLAY_DDCBLOCKACCESSGET          ADL_Display_DDCBlockAccess_Get;
	ADL_DISPLAY_DISPLAYINFO_GET					ADL_Display_DisplayInfo_Get;
	ADL_DISPLAY_EDIDDATA_GET						ADL_Display_EdidData_Get;
} ADLPROCS;

#define SETWRITESIZE 8
#define GETRQWRITESIZE 6
#define GETCAPWRITESIZE 7
#define GETREPLYWRITESIZE 1
#define GETREPLYREADSIZE 11
#define GETREPLYCAPSIZEFIXED 38
#define GETREPLYCAPSIZEVARIABLE 6
#define MAXREADSIZE 131

#define SET_VCPCODE_OFFSET 4
#define SET_HIGH_OFFSET 5
#define SET_LOW_OFFSET 6
#define SET_CHK_OFFSET 7

#define GETRQ_VCPCODE_OFFSET 4
#define GETRQ_CHK_OFFSET 5

#define GETRP_LENGHTH_OFFSET 1
#define GETRP_MAXHIGH_OFFSET 6
#define GETRP_MAXLOW_OFFSET 7
#define GETRP_CURHIGH_OFFSET 8
#define GETRP_CURLOW_OFFSET 9

#define CAP_HIGH_OFFSET 4
#define CAP_LOW_OFFSET 5
#define CAP_CHK_OFFSET 6

#define VCP_CODE_BRIGHTNESS 0x10
#define VCP_CODE_CONTRAST 0x12
#define VCP_CODE_COLORTEMP 0x14
#define VCP_CODE_CAPABILITIES 0xF3
#define VCP_CODE_CAPABILITIES_NEW 0xF4

