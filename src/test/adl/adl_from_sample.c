/*
 * adl_from_sample.c
 *
 * ADL DDC functions extracted from ADL sample code
 *
 *  Created on: Jul 27, 2014
 *      Author: rock
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <base/common.h>
#include <base/msg_control.h>
#include <util/string_util.h>

#include "adl/adl_impl/adl_sdk_includes.h"
#include "adl/adl_impl/adl_intf.h"
#include "adl/adl_impl/adl_friendly.h"

#include "test/adl/adl_from_sample.h"



#define MAX_NUM_DISPLAY_DEVICES 16

//********************************
// Globals
//********************************
UINT aAllConnectedDisplays[MAX_NUM_DISPLAY_DEVICES]; //int array of connected displays for each of the ATI devices (aligned with sDriverNames)
ADLPROCS adlprocs = {0,0,0,0};
UCHAR ucSetCommandWrite[SETWRITESIZE]               ={0x6e,0x51,0x84,0x03,0x00,0x00,0x00,0x00};
UCHAR ucGetCommandRequestWrite[GETRQWRITESIZE]      ={0x6e,0x51,0x82,0x01,0x00,0x00};
UCHAR ucGetCommandCapabilitiesWrite[GETCAPWRITESIZE]={0x6e,0x51,0x83,0xf3,0x00,0x00,0x00};
UCHAR ucGetCommandReplyWrite[GETREPLYWRITESIZE]     ={0x6f};
UCHAR ucGetCommandReplyRead[MAXREADSIZE];
UCHAR ucGlobalVcp = VCP_CODE_BRIGHTNESS;

LPAdapterInfo        lpAdapterInfo = NULL;
LPADLDisplayInfo  lpAdlDisplayInfo = NULL;
char                    MonitorNames[MAX_NUM_DISPLAY_DEVICES] [128];    // Array of Monitor names

bool TRUE = true;
bool FALSE = false;




//
// v functions from ADL sample code
//

// Function:
// void vGetCapabilitiesCommand
// Purpose:
// Get the MCCS capabilities
// Input: UCHAR ucVcp, VCP code (CONTRAST, BRIGHTNESS, etc)
//        int iDisplayIndex, display index
// Output: VOID
bool vGetCapabilitiesCommand(UCHAR ucVcp, int iAdapterIndex, int iDisplayIndex)
{
  unsigned int i;
  unsigned char chk=0;
  int val=0;
  int read_val=0;
  int temp_val=1;
  int replySize=0;
  bool ret=TRUE;

  ucGetCommandCapabilitiesWrite[CAP_LOW_OFFSET]=0;

  if (ucVcp == VCP_CODE_CAPABILITIES)
    replySize = GETREPLYCAPSIZEFIXED;
  else
    replySize = GETREPLYCAPSIZEVARIABLE;

  while(temp_val!=0)
  {
    // set the offset
    ucGetCommandCapabilitiesWrite[CAP_LOW_OFFSET]+=(UCHAR)val;
    // get checksum
    for ( i = 0; i < CAP_CHK_OFFSET; i++)
      chk=chk^ucGetCommandCapabilitiesWrite[i];

    ucGetCommandCapabilitiesWrite[CAP_CHK_OFFSET] = chk;

    // write get capability with offset
    vWriteI2c((char*)&ucGetCommandCapabilitiesWrite[0], GETCAPWRITESIZE, iAdapterIndex, iDisplayIndex);
    sleep_millis(40);
    // initial read to get the length to determine actual read length
    vWriteAndReadI2c((char*)&ucGetCommandReplyWrite[0], GETREPLYWRITESIZE, (char*)&ucGetCommandReplyRead[0], replySize,
                     iAdapterIndex, iDisplayIndex);

    if (ucVcp == VCP_CODE_CAPABILITIES_NEW)
    {
      // compute read length
      read_val= (int)ucGetCommandReplyRead[GETRP_LENGHTH_OFFSET] & 0x7F;
      read_val += 0x3;
      // re-write get capability with offset

      vWriteI2c((char*)&ucGetCommandCapabilitiesWrite[0],GETCAPWRITESIZE, iAdapterIndex, iDisplayIndex);
      sleep_millis(40);
      // read with actual read length computed from above
      vWriteAndReadI2c((char*)&ucGetCommandReplyWrite[0],GETREPLYWRITESIZE,(char*)&ucGetCommandReplyRead[0],read_val,
                       iAdapterIndex, iDisplayIndex);
    }
    if ((int)ucGetCommandReplyRead[GETRP_LENGHTH_OFFSET] == 0)
    {
      ret=FALSE;
      break;
    }
    // compute new offset
    val=(int)ucGetCommandReplyRead[GETRP_LENGHTH_OFFSET]-0x83;
    temp_val=val;
    chk=0;
  }

  return ret;
}



// Function:
// void vGetVcpCommand
// Purpose:
// Get the values from display based on which VCP code
// Input: UCHAR ucVcp, VCP code (CONTRAST, BRIGHTNESS, etc)
//        int*ulMaxVal, return value of the max possible value to can be set
//        int*ulCurVal, return value of current set
//        int iDisplayIndex, display index
// Output: VOID

int vGetVcpCommand(UCHAR ucVcp, UINT * ulMaxVal, UINT * ulCurVal, int iAdapterIndex, int iDisplayIndex)
{
  unsigned int i;
  unsigned char chk=0;
  int ADL_Err = ADL_ERR;
  printf("(%s) ucVcp=0x%02x, iAdapterIndex=%d, iDisplayIndex=%d\n", __func__, ucVcp, iAdapterIndex, iDisplayIndex );
  // known values for testing:
  *ulMaxVal = 2;
  *ulCurVal = 2;

  sleep_millis(500);    // added

  // adding initial write as per my posted example to i2c list
  // unsigned char zeroByte = 0x00;
  // int rc = vWriteI2c( &zeroByte, 1, iAdapterIndex, iDisplayIndex);
  // printf("(%s) vWriteI2c() of zeroByte returned %d   \n", __func__, rc );

  ucGetCommandRequestWrite[GETRQ_VCPCODE_OFFSET]=ucVcp;

  for( i = 0; i < GETRQ_CHK_OFFSET; i++)
      chk = chk ^ ucGetCommandRequestWrite[ i ];

  ucGetCommandRequestWrite[GETRQ_CHK_OFFSET] = chk;
  // printf("(%s) DDC command to write: %s\n", __func__, hexstring(ucGetCommandRequestWrite, GETRQWRITESIZE) );

  ADL_Err = vWriteI2c( (char*) &ucGetCommandRequestWrite[0], GETRQWRITESIZE, iAdapterIndex, iDisplayIndex);
  printf("(%s) vWriteI2c() returned %d\n", __func__, ADL_Err );
  sleep_millis(40);
  if (ADL_Err == 0) {
     // for debugging:
     // printf("(%s) sizeof(ucGetCommandReplyRead)=%d, MAXREADSIZE=%d\n", __func__, sizeof(ucGetCommandReplyRead), MAXREADSIZE );
     assert( sizeof(ucGetCommandReplyRead) == MAXREADSIZE);
     memset(ucGetCommandReplyRead, 0, sizeof(ucGetCommandReplyRead));
     ADL_Err = vWriteAndReadI2c( (char*)&ucGetCommandReplyWrite[0], GETREPLYWRITESIZE, (char*) &ucGetCommandReplyRead[0],
                              GETREPLYREADSIZE, iAdapterIndex, iDisplayIndex);
     printf("(%s) vWriteAndReadI2c() returned %d\n", __func__, ADL_Err );
     if (ADL_Err == 0) {
        char * hs = hexstring(ucGetCommandReplyRead, GETREPLYREADSIZE);
        printf("(%s) Data returned: %s  \n", __func__, hs );
        // hex_dump(ucGetCommandReplyRead, GETREPLYREADSIZE);
        free(hs);
     }
     *(ulMaxVal) = (ucGetCommandReplyRead[GETRP_MAXHIGH_OFFSET] << 8 |ucGetCommandReplyRead[GETRP_MAXLOW_OFFSET]);
     *(ulCurVal) = (ucGetCommandReplyRead[GETRP_CURHIGH_OFFSET] << 8 |ucGetCommandReplyRead[GETRP_CURLOW_OFFSET]);
  }
  // printf("(%s) Returning ADL_Err=%d  \n", __func__, ADL_Err );
  return ADL_Err;
}


//
// Function:
//  void vSetVcpCommand
// Purpose:
// Set the values from display based on which VCP code
// Input: UCHAR ucVcp, VCP code (CONTRAST, BRIGHTNESS, etc)
//       int ulVal, which value to set
//       int iDisplayIndex, display index
// Output: VOID
//
int vSetVcpCommand(UCHAR ucVcp, UINT ulVal, int iAdapterIndex, int iDisplayIndex)
{
  printf("(%s) Starting.  ucVcp=0x%02x, ulVal=%d   \n", __func__, ucVcp, ulVal );
  unsigned int i;
  unsigned char chk=0;
  int ADL_Err = ADL_ERR;

  ucSetCommandWrite[SET_VCPCODE_OFFSET] = ucVcp;
  ucSetCommandWrite[SET_LOW_OFFSET] = (char)(ulVal & 0x0ff);
  ucSetCommandWrite[SET_HIGH_OFFSET] = (char)((ulVal>>8) & 0x0ff);

  for( i = 0; i < SET_CHK_OFFSET; i++)
    chk=chk ^ ucSetCommandWrite[i];

  ucSetCommandWrite[SET_CHK_OFFSET] = chk;
  ADL_Err = vWriteI2c( (char*)&ucSetCommandWrite[0], SETWRITESIZE, iAdapterIndex, iDisplayIndex);
  printf("(%s) vWriteI2c() returned %d\n", __func__, ADL_Err );
  sleep_millis(50);
  return ADL_Err;
}


// Function:
// void vWriteI2c
// Purpose:
// Write to and read from an i2s address
// Input: char * lpucSendMsgBuf        Data to write
//       int iSendMsgLen               Length of data
//       int iAdapterIndex, int iDisplayIndex
// Output: result code
//
int vWriteI2c(char * lpucSendMsgBuf, int iSendMsgLen, int iAdapterIndex, int iDisplayIndex)
{
   sleep_millis(500);   // added
   int iRev = 0;
   // return adlprocs.ADL_Display_DDCBlockAccess_Get( iAdapterIndex, iDisplayIndex, 0, 0, iSendMsgLen, lpucSendMsgBuf, &iRev, NULL);
   // printf("(%s) iAdapterIndex=%d, iDisplayIndex=%d, receive buffer size = %d. receive buffer addr = NULL.  Writing: %s\n",
   //         __func__, iAdapterIndex, iDisplayIndex, iRev, hexstring(lpucSendMsgBuf, iSendMsgLen) );
   // hex_dump(lpucSendMsgBuf, iSendMsgLen);
   int rc = call_ADL_Display_DDCBlockAccess_Get( iAdapterIndex, iDisplayIndex, 0, 0, iSendMsgLen, lpucSendMsgBuf, &iRev, NULL);
   // printf("(%s) Returning %d\n", __func__, rc);
   return rc;
}


// Function:
// void vWriteAndReadI2c
// Purpose:
// Write to and read from an i2s address
// Input: char * lpucSendMsgBuf        Data to write
//       int iSendMsgLen               Length of data
//       char * lpucRecvMsgBuf         Read buffer
//       int iRecvMsgLen                  Read buffer size
//       int iAdapterIndex, int iDisplayIndex
// Output: result code
//
int vWriteAndReadI2c(char * lpucSendMsgBuf, int iSendMsgLen, char * lpucRecvMsgBuf, int iRecvMsgLen,  int iAdapterIndex, int iDisplayIndex)
{
   sleep_millis(500);   // added
   // return adlprocs.ADL_Display_DDCBlockAccess_Get( iAdapterIndex, iDisplayIndex, 0, 0,
   //                                                       iSendMsgLen, lpucSendMsgBuf, &iRecvMsgLen, lpucRecvMsgBuf);
   // printf("(%s) iAdapterIndex=%d, iDisplayIndex=%d, lpucRecvMsgBuf=%p, iRecvMsgLen=%d   Writing: %s\n",
   //         __func__, iAdapterIndex, iDisplayIndex, lpucRecvMsgBuf, iRecvMsgLen, hexstring(lpucSendMsgBuf, iSendMsgLen) );
   // hex_dump(lpucSendMsgBuf, iSendMsgLen);
   int rc = call_ADL_Display_DDCBlockAccess_Get( iAdapterIndex, iDisplayIndex, 0, 0,
                                                         iSendMsgLen, lpucSendMsgBuf, &iRecvMsgLen, lpucRecvMsgBuf);
   if (rc != 0) {
      char * hs = hexstring((Byte*)lpucRecvMsgBuf, iRecvMsgLen);
      printf("(%s) Value returned: %s  \n", __func__, hs );
      // hex_dump(lpucRecvMsgBuf, iRecvMsgLen);
      free(hs);
   }
   // printf("(%s) Returning %d\n", __func__, rc);
   return rc;
}

