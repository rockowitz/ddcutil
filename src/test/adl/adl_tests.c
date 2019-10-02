/* adl_tests.c
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

#include <assert.h>
#include <stdlib.h>              // for wchar_t in adl_structures.h
#include <stdio.h>
#include <stdbool.h>

#include "util/string_util.h"
#include "util/report_util.h"

#include "base/core.h"
#include "base/ddc_packets.h"
#include "base/parms.h"
#include "base/sleep.h"

#include "adl/adl_impl/adl_sdk_includes.h"
#include "adl/adl_impl/adl_intf.h"
#include "adl/adl_impl/adl_friendly.h"
#include "adl/adl_impl/adl_report.h"
#include "adl/adl_impl/adl_aux_intf.h"

#include "test/adl/adl_from_sample.h"
#include "test/adl/adl_tests.h"


#pragma GCC diagnostic ignored "-Wpointer-sign"


void get_luminosity_amd_sample(int adapterNdx, int displayNdx, int how, int sendOffset, bool sendChecksum) {
   printf("(%s) Starting adapterNdx=%d, displayNdx=%d, how=%d, sendOffset=%d, sendChecksum=%d\n",
          __func__, adapterNdx, displayNdx, how, sendOffset, sendChecksum );

   // DDC_Packet * response_packet_ptr = NULL;
   // Byte luminosity_op_code = 0x10;
   int rc;

#ifdef MAYBE
   unsigned char zeroBytes[4] = {0};  // 0x00;
   rc = write(fh, &zeroBytes[0], 1);     // succeeds if <= 2 bytes, fails if >= 3
   if (rc < 0) {
      printf("(%s) Bus reset failed. rc=%d, errno=%s. Terminating execution.\n", __func__, rc, errno_name(errno) );
      exit(1);
   }
   printf("(%s) Initial write succeeded\n", __func__);
#endif

   unsigned char ddc_cmd_bytes[] = {
      0x6e,              // address 0x37, shifted left 1 bit
      0x51,              // source address
      0x02 | 0x80,       // number of DDC data bytes, with high bit set
      0x01,              // DDC Get Feature Command
      0x10,              // Luminosity feature code
      0x00,              // checksum, to be set
   };
   ddc_cmd_bytes[5] = ddc_checksum(ddc_cmd_bytes, 5, false);    // calculate DDC checksum on all bytes
   assert(ddc_cmd_bytes[5] == 0xac);


   Byte readbuf[256];
   int  receivedCt;

   //   rc = 0;

   if ( how == 0 ) {
      int sendCt = 6 - sendOffset;
      if (!sendChecksum)
         sendCt -= 1;
      printf("(%s) how=%d, sendOffset=%d, sendCt=%d. sendChecksum=%d\n", __func__, how, sendOffset, sendCt, sendChecksum );

      // sendOffset=1, sendChecksum=true:   rc=-3

      // rc = write(fh, ddc_cmd_bytes+1, sizeof(ddc_cmd_bytes)-1);
      ADLI2C adli2c;
      adli2c.iSize = sizeof(adli2c);
      // ADL_DL_I2C_LINE_OEM, ADL_DL_LINE_OD_CONTROL, ADL_DL_LINE_OEM2 3,4,5,6
      adli2c.iLine = ADL_DL_I2C_LINE_OEM;    // ?? numerical value representing hardware I2c  related to no iDisplay on arg?
      adli2c.iAddress = ddc_cmd_bytes[0];
      // adli2c.iAddress = 0x37;
      adli2c.iOffset = 0;   // ???
      adli2c.iAction = ADL_DL_I2C_ACTIONWRITE;
      adli2c.iSpeed = 50;    // ???  I2C clock speed
      adli2c.iDataSize = sendCt; // number of bytes to be send or received
      adli2c.pcData = ddc_cmd_bytes + sendOffset;  //  ???
      rc = adl->ADL_Display_WriteAndReadI2C(adapterNdx, &adli2c);
      printf("(%s) ADL_Display_WriteAndReadI2C returned %d\n", __func__, rc );

      if (rc == 0) {
         // usleep(DEFAULT_TIMEOUT);
         sleep_millis_with_tracex(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, __LINE__, __FILE__, NULL);

         ADLI2C adli2cResponse;
         adli2cResponse.iSize = sizeof(adli2cResponse);
         adli2cResponse.iLine = 0;    // ?? numerical value representing hardware I2c
         adli2cResponse.iAddress = 0x6e;
         adli2cResponse.iOffset = 0;   // ???
         adli2cResponse.iAction = ADL_DL_I2C_ACTIONREAD;
         adli2cResponse.iSpeed = 0;    // ???  I2C clock speed
         adli2cResponse.iDataSize = 16; // wrong  number of bytes to be send or received   ???
         adli2cResponse.pcData = readbuf;
         rc = adl->ADL_Display_WriteAndReadI2C(adapterNdx, &adli2c);
         printf("(%s) ADL_Display_WriteAndReadI2C returned %d\n", __func__, rc );
         if (rc == 0) {
            hex_dump(readbuf, 16);
         }
      }
   }
   else if (how == 1) {

      int sendCt = 6 - sendOffset;
      if (!sendChecksum)
         sendCt -= 1;
      printf("(%s) how=%d, sendOffset=%d, sendCt=%d. sendChecksum=%d\n", __func__, how, sendOffset, sendCt, sendChecksum );

      receivedCt = sizeof(readbuf);

      //sendOffset = 2, sendChecksum=true, rc = -1

      // rc = i2c_smbus_write_i2c_block_data(fh, ddc_cmd_bytes[1], sizeof(ddc_cmd_bytes)-2, ddc_cmd_bytes+2);
      rc = call_ADL_Display_DDCBlockAccess_Get(
          adapterNdx,
          displayNdx,
          0,           // iOption  ADL_DDC_OPTION_SWITCHDDC2, ADL_DDC_RESTORECOMMAND
          0,           // iCommandIndex, 0 unless flag ADL_DDC_RESTORECOMMAND set
          sendCt,           // iSendMsgLen
          ddc_cmd_bytes+sendOffset, //   char * lpucSendMsgBuf  start where?  include checksum?
          &receivedCt,
          readbuf);
      printf("(%s) ADL_Display_DDCBlockAccess_Get() returned %d\n", __func__, rc );
      if (rc == 0) {
         hex_dump(readbuf, 16);
      }
   }


   else {
      printf("(%s) processing how=2   \n", __func__ );
      int sendCt = 6 - sendOffset;
      if (!sendChecksum)
         sendCt -= 1;
      printf("(%s) how=%d, sendOffset=%d, sendCt=%d. sendChecksum=%d\n", __func__, how, sendOffset, sendCt, sendChecksum );
      char * s = hexstring(ddc_cmd_bytes+sendOffset, sendCt);
      printf("(%s) Writing: %s   \n", __func__, s );
      free(s);

      Byte readbuf2[MAXREADSIZE];
      int rcvCt = MAXREADSIZE;

      /* rc = */  adl_ddc_write_read_with_retry(
              adapterNdx,
              displayNdx,
              ddc_cmd_bytes+sendOffset,
              sendCt,
              readbuf2,
              &rcvCt);

      rc = adl_ddc_write_only_with_retry(adapterNdx, displayNdx, ddc_cmd_bytes+sendOffset, sendCt);


      if (rc == 0) {
         printf("(%s) Data returned:  \n", __func__ );
         hex_dump(readbuf2, rcvCt);

           // *(ulMaxVal) = (ucGetCommandReplyRead[GETRP_MAXHIGH_OFFSET] << 8 |ucGetCommandReplyRead[GETRP_MAXLOW_OFFSET]);
           // *(ulCurVal) = (ucGetCommandReplyRead[GETRP_CURHIGH_OFFSET] << 8 |ucGetCommandReplyRead[GETRP_CURLOW_OFFSET]);

      }

   }


#ifdef WORKS
   if (rc >= 0) {
        Byte * readbuf = (Byte *)calloc(sizeof(unsigned char),256);
        // Byte cmd_byte = 0x6e;
        printf("(%s) callling call_read   \n", __func__ );
        rc = call_read(fh, readbuf, 32, true);
        if (rc < 0) {
           printf("(%s) call_read returned %d, errno=%d. Terminating execution  \n", __func__, rc, errno );
           exit(1);
        }
        printf("(%s) call_read() returned %d   \n", __func__, rc );
        if (rc >= 0) {
           hex_dump(readbuf, rc);

           int rc2 = create_ddc_getvcp_response_packet(
                           readbuf, 32, luminosity_op_code, "get_vcp:response packet", &response_packet_ptr);
           printf("(%s) create_ddc_getvcp_response_packet() returned %d\n", __func__, rc2);
           if (rc2 == 0) {
              Interpreted_Vcp_Code * interpretation_ptr = NULL;
              rc2 = get_interpreted_vcp_code(response_packet_ptr, false, &interpretation_ptr);
              if (rc2 == 0) {
                 printf("(%s) interpretation_ptr=%p\n", __func__, interpretation_ptr);
                 report_interpreted_vcp_code(interpretation_ptr);
              }
              // read_ok = true;
           }
        } // read_ok
     } // write_ok
#endif

#ifdef TODO
        hex_dump(readbuf,1+rc);
        assert(readbuf[1] == 0x6e);
        int ddc_data_length = readbuf[2] & 0x7f;
        assert(ddc_data_length == 8);
        assert(readbuf[3] == 0x02);       // get feature response


        readbuf[0] = 0x50;   // for calculating DDC checksum
        unsigned char calculated_checksum = ddc_checksum(readbuf, 11, false);
        if (readbuf[11] != calculated_checksum) {
           printf("(%s) Unexpected checksum.  actual=0x%02x, calculated=0x%02x  \n", __func__, readbuf[11], calculated_checksum );
        }

        int max_val = (readbuf[7] << 8) + readbuf[8];
        int cur_val = (readbuf[9] << 8) + readbuf[10];

        printf("(%s) cur_val = %d, max_val = %d   \n", __func__, cur_val, max_val );

     }






     if (response_packet_ptr)
        free_ddc_packet(response_packet_ptr);

#endif

}



void exercise_ad_calls(int iAdapterIndex, int iDisplayIndex) {
   int rc;
   int iColorCaps;
   int iCurrent, iDefault, iMin, iMax, iStep;
   int iValidBits;
   ADLDisplayEDIDData edid_data;
   ADLDDCInfo2 ddc_info2;

   printf("(%s) iAdapterIndex=%d, iDisplayIndex=%d\n", __func__, iAdapterIndex, iDisplayIndex );

   rc = adl->ADL_Display_ColorCaps_Get( iAdapterIndex, iDisplayIndex, &iColorCaps, &iValidBits);
   printf("(%s) ADL_DisplayColorCaps returned %d\n", __func__, rc );

   rc = adl->ADL_Display_Color_Get( iAdapterIndex, iDisplayIndex, ADL_DISPLAY_COLOR_BRIGHTNESS,                                                        &iCurrent, &iDefault, &iMin, &iMax, &iStep);
   printf("(%s) ADL_Display_Color_Get() returned %d\n", __func__, rc );

   edid_data.iSize = sizeof(ADLDisplayEDIDData);
   edid_data.iFlag = 0;
   edid_data.iBlockIndex = 0;   // critical
   rc = adl->ADL_Display_EdidData_Get(iAdapterIndex, iDisplayIndex, &edid_data);
   printf("(%s) ADL_Display_EdidData() returned %d\n", __func__, rc );

   rc = adl->ADL_Display_DDCInfo2_Get(iAdapterIndex, iDisplayIndex, &ddc_info2);
   printf("(%s) ADL_Display_DDCInfo2_Get() returned %d\n", __func__, rc );
}


void diddle_adl_brightness(int iAdapterIndex, int iDisplayIndex) {
   printf("(%s) Starting. iAdapterIndex=%d, iDisplayIndex=%d\n", __func__, iAdapterIndex, iDisplayIndex );

   int  iColorCaps;
   int  iValidBits;
   int  iCurrent, iDefault, iMin, iMax, iStep;
   int  rc;

   adl->ADL_Display_ColorCaps_Get( iAdapterIndex, iDisplayIndex, &iColorCaps, &iValidBits);

   // Use only the valid bits from iColorCaps
   iColorCaps &= iValidBits;

   // Check if the display supports this particular capability
   if ( ADL_DISPLAY_COLOR_BRIGHTNESS & iColorCaps ) {
      // Get the Current display Brightness, Default value, Min, Max and Step
       rc = adl->ADL_Display_Color_Get( iAdapterIndex, iDisplayIndex, ADL_DISPLAY_COLOR_BRIGHTNESS,
                                                 &iCurrent, &iDefault, &iMin, &iMax, &iStep);
       printf("(%s) ADL_Display_Color_Get() returned %d\n", __func__, rc );
       if (rc == ADL_OK) {
         printf("(%s) Adjusting brightness.  iAdapterIndex=%d, iDisplayIndex=%d  \n", __func__, iAdapterIndex, iDisplayIndex );
         // Set half of the Min brightness for .5 sec
         rc = adl->ADL_Display_Color_Set( iAdapterIndex, iDisplayIndex, ADL_DISPLAY_COLOR_BRIGHTNESS, iMin / 2 );
         printf("(%s) ADL_Display_Color_Set returned %d\n", __func__, rc );
         sleep_millis( 500 );
         rc = adl->ADL_Display_Color_Set( iAdapterIndex, iDisplayIndex, ADL_DISPLAY_COLOR_BRIGHTNESS, iCurrent );
         printf("(%s) ADL_Display_Color_Set returned %d\n", __func__, rc );
         sleep_millis( 500 );
         // Set a quarter of the Max brightness for .5 sec
         rc = adl->ADL_Display_Color_Set( iAdapterIndex, iDisplayIndex, ADL_DISPLAY_COLOR_BRIGHTNESS, iMax / 4 );
         printf("(%s) ADL_Display_Color_Set returned %d\n", __func__, rc );
         sleep_millis( 500 );
         // Restore the current brightness of the display
         rc = adl->ADL_Display_Color_Set( iAdapterIndex, iDisplayIndex, ADL_DISPLAY_COLOR_BRIGHTNESS, iCurrent );
         printf("(%s) ADL_Display_Color_Set returned %d\n", __func__, rc );
         sleep_millis( 500 );
      }
   }
   printf("(%s) Done   \n", __func__ );
}



void adapterDisplayProbeLoop(int maxAdapters, int maxDisplays) {

  puts("\n----------------------------------------------------------------------------");
  puts("\nIterating over adapter and display numbers:");
  ADLDisplayEDIDData edid_data;
  ADLDDCInfo2 ddc_info2;
  int rc;
  int iAd, iDis;
  for (iAd=0; iAd < maxAdapters; iAd++) {
     for (iDis=0; iDis < maxDisplays; iDis++) {
        printf("iAd=%d, iDis=%d\n", iAd, iDis);
        edid_data.iSize = sizeof(ADLDisplayEDIDData);
        edid_data.iFlag = 0;
        edid_data.iBlockIndex = 0;   // critical
        rc = adl->ADL_Display_EdidData_Get(iAd, iDis, &edid_data);
        if (rc != ADL_OK) {
           printf("(%s) ADL_Display_EdidData_Get() returned %d\n", __func__, rc );
        }
        else {
           puts("EdidData_Get succeeded");
           report_adl_ADLDisplayEDIDData(&edid_data,1);
        }

        rc = adl->ADL_Display_DDCInfo2_Get(iAd, iDis, &ddc_info2);
        if (rc != ADL_OK) {
           printf("(%s) ADL_Display_DDCInfo2_Get() returned %d\n", __func__, rc );
        }
        else {
           puts("ADL_DISPLAY_DDCInfo2_Get succeeded");
           report_adl_ADLDDCInfo2(&ddc_info2, false /* verbose */, 1);
        }

        int iMajor, iMinor;
        rc = adl->ADL_Display_WriteAndReadI2CRev_Get(iAd, &iMajor, &iMinor);
        if (rc != ADL_OK) {
           printf("(%s) ADL_Display_WroteAmdReadI2CRev_Get() returned %d\n", __func__, rc );
        }
        else {
           printf("ADL_DISPLAY_WroteAmdReadO2CRev_Get succeeded.  I2C rev = %d.%d\n", iMajor, iMinor);
        }

        int ulMaxVal;
        int ulCurVal;
        rc = vGetVcpCommand(0x10, &ulMaxVal,  &ulCurVal, iAd, iDis);
        if (rc != ADL_OK) {
           printf("(%s) vGetVcpCommand() returned %d\n", __func__, rc );
        }
        else {
           printf("vGetVcpCommand() succeeded.  ulMaxVal=%d, ulCurVal=%d\n", ulMaxVal, ulCurVal);
        }
     }
  }
}



int get_luminosity_using_vGetVcpCommand(int iAdapterIndex, int iDisplayIndex, int * pmaxval, int * pcurval) {
   printf("(%s) Getting luminosity using vGetVcpCommand()   \n", __func__ );
   int rc = vGetVcpCommand(0x10,  pmaxval, pcurval, iAdapterIndex, iDisplayIndex);
   printf("(%s) vGetVcpCommand returned %d\n", __func__, rc );
   if (rc == 0) {
      printf("(%s) *pmaxval=%d, *pcurval=%d   \n", __func__, *pmaxval, *pcurval );
   }
   sleep_millis(500);
   return rc;
}



int set_luminosity_using_vSetVcpCommand(int iAdapterIndex, int iDisplayIndex, int newval) {
   printf("(%s) Setting luminosity = %d using vSetVcpCommand()   \n", __func__, newval );
   int rc = vSetVcpCommand(0x10,  newval, iAdapterIndex, iDisplayIndex);
   printf("(%s) vSetVcpCommand returned %d  \n", __func__, rc );
   sleep_millis(500);
   return rc;
}


void run_adapter_display_tests() {
   printf("(%s) Starting\n", __func__ );
   int ndx = 0;
   // for (ndx = 0; ndx < activeDisplayCt2; ndx++) {
   for (ndx = 0; ndx < active_display_ct; ndx++) {

      puts("");
      // printf("(%s) activeDisplay loop, ndx=%d   \n", __func__, ndx );
      ADL_Display_Rec * pDisp = &active_displays[ndx];
      report_adl_display_rec(pDisp, false /* verbose */, 0);
      puts("");

      int iAdapterIndex = pDisp->iAdapterIndex;
      int iDisplayIndex = pDisp->iDisplayIndex;
      printf("(%s) iAdapterIndex=%d, iDisplayIndex=%d\n", __func__, iAdapterIndex, iDisplayIndex );

      printf("(%s) -------------> exercise_ad_calls\n", __func__ );
      exercise_ad_calls(iAdapterIndex, iDisplayIndex);
      puts("");

      // printf("(%s) -------------> diddleBrightness  \n", __func__ );
      // diddleBrightness(iAdapterIndex, iDisplayIndex);

      puts("");
      printf("(%s) -------------> using vGetVcpCommand, vSetVcpCommand   \n", __func__ );
      // unsigned char vcpCode = 0x10;
      // unsigned int maxval = 2;   // funny number for testing
      // unsigned int curval = 2;

      // get_luminosity_using_vGetVcpCommand(iAdapterIndex, iDisplayIndex, &maxval, &curval);
      // set_luminosity_using_vSetVcpCommand(iAdapterIndex, iDisplayIndex, 25);
      // get_luminosity_using_vGetVcpCommand(iAdapterIndex, iDisplayIndex, &maxval, &curval);
      set_luminosity_using_vSetVcpCommand(iAdapterIndex, iDisplayIndex, 230);
      // get_luminosity_using_vGetVcpCommand(iAdapterIndex, iDisplayIndex, &maxval, &curval);
      puts("");

      printf("(%s) ---------------> using amd_adl_getVCP, onecall=false \n", __func__ );
      // how, sendOffset, sendChecksum
      // get_luminosity_amd_sample(iAdapterIndex, iDisplayIndex, 2, 0, true);

      adl_ddc_set_vcp(iAdapterIndex, iDisplayIndex, 0x10, 225);
      // adl_DDC_getVCP(iAdapterIndex, iDisplayIndex, 0x10, false);
      puts("");

      // printf("(%s) ---------------> using amd_adl_getVCP, onecall=true \n", __func__ );
      // adl_DDC_getVCP(iAdapterIndex, iDisplayIndex, 0x10, true);
      puts("");

      printf("(%s) -------------> exercise_ad_calls\n", __func__ );
      exercise_ad_calls(iAdapterIndex, iDisplayIndex);
      puts("");
   }
}


void adl_testmain() {
   adl_initialize();
   if (active_display_ct > 0) {
      printf("(%s) activeDisplayCt2=%d\n", __func__, active_display_ct );
      run_adapter_display_tests();
   }
   adl_release();
}


#ifdef OLD


bool init_adl() {
   printf("(%s) Starting.\n", __func__ );
   int rc;
   // Initialize ADL. The second parameter is 1, which means:
   // retrieve adapter information only for adapters that are physically present and enabled in the system
   printf("(%s) adl=%p\n", __func__, adl );
   printf("(%s) adl->ADL_Main_Control_Create=%p   \n", __func__, adl->ADL_Main_Control_Create );
   rc = adl->ADL_Main_Control_Create (ADL_Main_Memory_Alloc, 1);
   if (rc != ADL_OK) {
      printf("ADL Initialization Error! ADL_Main_Control_Create() returned: %d.\n", rc);
      return false;
   }
   printf("(%s) ADL_Main_Control_Create succeeded.\n", __func__ );

   // Obtain the number of adapters for the system
   rc = adl->ADL_Adapter_NumberOfAdapters_Get ( &iNumberAdapters );
   if (rc != ADL_OK) {
      printf("Cannot get the number of adapters!  ADL_Adapter_NumberOfAdapaters_Get() returned %d\n", rc);
      return false;
   }
   printf("(%s) Found %d adapters\n", __func__, iNumberAdapters );

   if ( 0 < iNumberAdapters ) {
      lpAdapterInfo = malloc ( sizeof (AdapterInfo) * iNumberAdapters );
      memset ( lpAdapterInfo,'\0', sizeof (AdapterInfo) * iNumberAdapters );

      // Get the AdapterInfo structure for all adapters in the system
      adl->ADL_Adapter_AdapterInfo_Get (lpAdapterInfo, sizeof (AdapterInfo) * iNumberAdapters);
   }

   // Repeat for all available adapters in the system
   int iAdapterNo, iDisplayNo;
   int iAdapterIndex;
   int iDisplayIndex;
   int  iCurrent, iDefault, iMin, iMax, iStep;
   for ( iAdapterNo = 0; iAdapterNo < iNumberAdapters; iAdapterNo++ ) {
      printf("Adapter: %d\n", iAdapterNo );
      reportAdapterInfo(&lpAdapterInfo[ iAdapterNo], 0);
      iAdapterIndex = lpAdapterInfo[ iAdapterNo ].iAdapterIndex;
      assert(iAdapterIndex == iAdapterNo);
      printf("(%s) iAdapterIndex=%d\n", __func__, iAdapterIndex );
      ADL_Main_Memory_Free ( &lpAdlDisplayInfo );
      rc = adl->ADL_Display_DisplayInfo_Get (
              lpAdapterInfo[iAdapterNo].iAdapterIndex,
              &iNumDisplays,
              &lpAdlDisplayInfo,
              0);
      if (rc != ADL_OK) {
         printf("ADL_Display_DisplayInfo_Get() returned %d\n", rc);
         continue;
      }

      for ( iDisplayNo = 0; iDisplayNo < iNumDisplays; iDisplayNo++ ) {
         printf("(%s) adapter number = %d,   display number: %d  \n", __func__, iAdapterNo, iDisplayNo );
         reportADLDisplayInfo(&lpAdlDisplayInfo[iDisplayNo], 0);

         // puts("Wolf 2");
         // For each display, check its status. Use the display only if it's connected AND mapped (iDisplayInfoValue: bit 0 and 1 )
          if (  ( ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED ) !=
                ( (ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED) &
                      lpAdlDisplayInfo[ iDisplayNo ].iDisplayInfoValue )
             )
          {
             printf("(%s) Display %d not connected or not mapped\n", __func__, iDisplayNo );
             continue;   // Skip the not connected or not mapped displays
          }

          // Is the display mapped to this adapter? This test is too restrictive and may not be needed.
          if ( iAdapterIndex != lpAdlDisplayInfo[ iDisplayNo ].displayID.iDisplayLogicalAdapterIndex ) {
             printf("(%s) Display not mapped to this adapter   \n", __func__ );
             continue;
          }

          iDisplayIndex = lpAdlDisplayInfo[ iDisplayNo ].displayID.iDisplayLogicalIndex;

          assert(activeDisplayCt < MAX_ACTIVE_DISPLAYS);
          activeDisplays[activeDisplayCt].iAdapterNo = iAdapterNo;
          activeDisplays[activeDisplayCt].iDisplayNo = iDisplayNo;
          activeDisplays[activeDisplayCt].iLogicalDisplayIndex = iDisplayIndex;
          activeDisplays[activeDisplayCt].displayID = lpAdlDisplayInfo[iDisplayNo].displayID;
          activeDisplayCt++;

          ADLDisplayEDIDData edid_data;

          // rc == -3:
          // rc = ADL_Display_EdidData_Get(iAdapterIndex, iDisplayIndex, &edid_data);
          // also rc == -3:
          rc = adl->ADL_Display_EdidData_Get(iAdapterNo, iDisplayNo, &edid_data);
          if (rc != ADL_OK) {
             printf("(%s) ADL_Display_EdidData_Get() returned %d\n", __func__, rc );
          }
          else {
             puts("EdidData_Get succeeded");
             report_adl_ADLDisplayEDIDData(&edid_data,1);
          }

          diddle_adl_brightness(iAdapterIndex, iDisplayIndex);


      }   // iNumDisplays
   }      // iNumberAdapters


   adapterDisplayProbeLoop(iNumberAdapters, 6);


   puts("--------------------------------------------------------------------");
   printf("(%s) Active displays:  \n", __func__ );
   int activeDisplayNdx;
   for (activeDisplayNdx = 0; activeDisplayNdx < activeDisplayCt; activeDisplayNdx++) {
      printf("activeDisplayNdx=%d\n", activeDisplayNdx);
      ActiveDisplay * pad = &activeDisplays[activeDisplayNdx];
      printf("   adapterNo=%d\n", pad->iAdapterNo);
      printf("   displayNo=%d\n", pad->iDisplayNo);
      printf("   logicalDisplayIndex=%d\n", pad->iLogicalDisplayIndex);
      reportADLDisplayID( &(pad->displayID), 2);

      int adNdx = pad->iAdapterNo;
      int dispNdx = pad->displayID.iDisplayLogicalIndex;

      printf("   Retrieving DDCInfo2 using adNdx=%d, dispNdx=%d\n", adNdx, dispNdx);
      rc = adl->ADL_Display_DDCInfo2_Get(adNdx, dispNdx, &ddc_info2);
      if (rc != ADL_OK) {
         printf("(%s) ADL_Display_DDCInfo2_Get() returned %d\n", __func__, rc );
      }
      else {
         puts("ADL_DISPLAY_DDCInfo2_Get succeeded");
         report_adl_ADLDDCInfo2(&ddc_info2, false /* verbose */, 1);
      }

      edid_data.iSize = sizeof(edid_data);
      edid_data.iFlag = 0;
      edid_data.iBlockIndex = 0;

      rc = adl->ADL_Display_EdidData_Get(adNdx, dispNdx, &edid_data);
      if (rc != ADL_OK) {
         printf("(%s) ADL_Display_EdidData_Get() returned %d\n", __func__, rc );
      }
      else {
         puts("EdidData_Get succeeded");
         report_adl_ADLDisplayEDIDData(&edid_data,1);
     }


     // get_luminosity_amd_sample(adNdx, dispNdx, 0);
     //                                        how, startOffset, sendChecksum
     get_luminosity_amd_sample(adNdx, dispNdx, 0,   0,           true);
     get_luminosity_amd_sample(adNdx, dispNdx, 0,   1,           true);
     get_luminosity_amd_sample(adNdx, dispNdx, 0,   2,           true);
     get_luminosity_amd_sample(adNdx, dispNdx, 1,   0,           true);

     UCHAR ucVcp = 0x10;
     UINT ulMaxVal;
     UINT ulCurVal;
     vGetVcpCommand(ucVcp, &ulMaxVal, &ulCurVal, adNdx, dispNdx);
     printf("(%s) vGetVcpCommand() returned ulMaxVal=%d, ulCurVal=%d\n", __func__, ulMaxVal, ulCurVal );
     // bool ok = vGetCapabilitiesCommand(VCP_CODE_CAPABILITIES, adNdx, dispNdx);
     // printf("(%s) vGetCapabilities() returned %d\n", __func__ , ok);

   }

   ADL_Main_Memory_Free ( &lpAdapterInfo );
   ADL_Main_Memory_Free ( &lpAdlDisplayInfo );

   return true;
}
#endif

