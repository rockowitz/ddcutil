/*  adl_services.c
 *
 *  Created on: Oct 30, 2015
 *      Author: rock
 *
 *  Functions in this file were originally part of adl_inf.c,
 *  but with code refactoring are now only called from tests.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <base/common.h>
#include <base/ddc_packets.h>
#include <base/parms.h>
#include <util/string_util.h>
#include <base/util.h>

#include "adl/adl_shim.h"
#include "adl/adl_impl/adl_friendly.h"
#include "adl/adl_impl/adl_intf.h"

#include "adl/adl_impl/adl_aux_intf.h"


// 10/2015: only used in adl_tests.c
Base_Status_ADL adl_ddc_write_only_with_retry(
      int     iAdapterIndex,
      int     iDisplayIndex,
      Byte *  pSendMsgBuf,
      int     sendMsgLen)
{
   if (adl_debug) {
      char * s = hexstring(pSendMsgBuf, sendMsgLen);
      printf("(%s) Starting. iAdapterIndex=%d, iDisplayIndex=%d, sendMsgLen=%d, pSendMsgBuf->%s\n",
             __func__, iAdapterIndex, iDisplayIndex, sendMsgLen, s);
      free(s);
   }

   int  maxTries = 2;
   int  tryctr = 0;
   bool can_retry = true;
   Base_Status_ADL  rc;

   for (tryctr=0; tryctr < maxTries && can_retry; tryctr++) {
      rc = adl_ddc_write_only(iAdapterIndex, iDisplayIndex, pSendMsgBuf, sendMsgLen);
      if (rc == -1) {
         sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, "after adl_DDC_write_only");
      }
      else
         can_retry=false;
   }

   if (adl_debug)
      printf("(%s) Returning %d.  tryctr=%d\n", __func__, rc, tryctr);
   return rc;
}


// 10/2015: only used in adl_tests.c
Base_Status_ADL adl_ddc_write_read_with_retry(
      int     iAdapterIndex,
      int     iDisplayIndex,
      Byte *  pSendMsgBuf,
      int     sendMsgLen,
      Byte *  pRcvMsgBuf,
      int *   pRcvBytect)
{
   bool debug = true;
   if (debug) {
      char * s = hexstring(pSendMsgBuf, sendMsgLen);
      printf("(%s) Starting. iAdapterIndex=%d, iDisplayIndex=%d, sendMsgLen=%d, pSendMsgBuf->%s, *pRcvBytect=%d\n",
             __func__, iAdapterIndex, iDisplayIndex, sendMsgLen, s, *pRcvBytect);
      free(s);
   }

   int  maxTries = 2;
   int  tryctr = 0;
   bool can_retry = true;
   Base_Status_ADL  rc;

   for (tryctr=0; tryctr < maxTries && can_retry; tryctr++) {
      rc = adl_ddc_write_read(iAdapterIndex, iDisplayIndex, pSendMsgBuf, sendMsgLen, pRcvMsgBuf, pRcvBytect);
      if (rc == -1) {
         sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_DEFAULT, __func__, "after adl_DDC_write_read");
      }
      else
         can_retry=false;
   }

   if (debug)
      printf("(%s) Returning %d.  tryctr=%d\n", __func__, rc, tryctr);
   return rc;
}

// 10/2015: unused
Base_Status_ADL adl_ddc_write_read_with_retry_onecall(
      int     iAdapterIndex,
      int     iDisplayIndex,
      Byte *  pSendMsgBuf,
      int     sendMsgLen,
      Byte *  pRcvMsgBuf,
      int *   pRcvBytect)
{
   if (adl_debug) {
      char * s = hexstring(pSendMsgBuf, sendMsgLen);
      printf("(%s) Starting. iAdapterIndex=%d, iDisplayIndex=%d, sendMsgLen=%d, pSendMsgBuf->%s, *pRcvBytect=%d\n",
             __func__, iAdapterIndex, iDisplayIndex, sendMsgLen, s, *pRcvBytect);
      free(s);
   }

   int  maxTries = 2;
   int  tryctr = 0;
   bool can_retry = true;
   Base_Status_ADL  rc;

   for (tryctr=0; tryctr < maxTries && can_retry; tryctr++) {
      rc = adl_ddc_write_read_onecall(iAdapterIndex, iDisplayIndex, pSendMsgBuf, sendMsgLen, pRcvMsgBuf, pRcvBytect);
      if (rc == -1) {
         sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_RETRY, __func__, "retry timeout");
      }
      else
         can_retry=false;
   }

   if (adl_debug)
      printf("(%s) Returning %d.  tryctr=%d\n", __func__, rc, tryctr);
   return rc;
}


//
// Top level functions to read and write VCP values
//

typedef
struct {
   Byte   mHi;
   Byte   mLo;
   Byte   sHi;
   Byte   sLo;
} Raw_GetVCP_Response_Data;


// 10/2015 unused
Base_Status_ADL adl_ddc_get_vcp(int iAdapterIndex, int iDisplayIndex, Byte vcp_feature_code, bool onecall) {
   if (adl_debug) {
      printf("(%s) Starting adapterNdx=%d, displayNdx=%d, vcp_feature_code=0x%02x\n",
             __func__, iAdapterIndex, iDisplayIndex, vcp_feature_code );
   }

   unsigned char ddc_cmd_bytes[] = {
      0x6e,              // address 0x37, shifted left 1 bit
      0x51,              // source address
      0x02 | 0x80,       // number of DDC data bytes, with high bit set
      0x01,              // DDC Get Feature Command
      vcp_feature_code,  //
      0x00,              // checksum, to be set
   };
   ddc_cmd_bytes[5] = ddc_checksum(ddc_cmd_bytes, 5, false);    // calculate DDC checksum on all bytes
   // assert(ddc_cmd_bytes[5] == 0xac);    // correct value for feature code 0x10

   int sendCt = sizeof(ddc_cmd_bytes);
   assert (sendCt == 6);

   Byte rcvBuf[32];
   int  rcvCt = 16;
   Base_Status_ADL rc;

   if (adl_debug) {
      char * s = hexstring(ddc_cmd_bytes, sendCt);
      printf("(%s) Writing: %s   \n", __func__, s );
      free(s);
   }

   if (onecall)
      rc = adl_ddc_write_read_onecall(     // alt: call "with_retry" version
              iAdapterIndex,
              iDisplayIndex,
              ddc_cmd_bytes,
              sendCt,
              rcvBuf,
              &rcvCt);

   else
      rc = adl_ddc_write_read(           // alt: call "with_retry" version
              iAdapterIndex,
              iDisplayIndex,
              ddc_cmd_bytes,
              sendCt,
              rcvBuf,
              &rcvCt);

   if (rc == 0) {
      if (adl_debug) {
         printf("(%s) Data returned:  \n", __func__ );
         hex_dump(rcvBuf, rcvCt);
      }

      // Need to perform error checking on result

      // *(ulMaxVal) = (ucGetCommandReplyRead[GETRP_MAXHIGH_OFFSET] << 8 |ucGetCommandReplyRead[GETRP_MAXLOW_OFFSET]);
      // *(ulCurVal) = (ucGetCommandReplyRead[GETRP_CURHIGH_OFFSET] << 8 |ucGetCommandReplyRead[GETRP_CURLOW_OFFSET]);
   }
   return rc;
}

// 10/2015: only used in tests
Base_Status_ADL adl_ddc_set_vcp(int iAdapterIndex, int iDisplayIndex, Byte vcp_feature_code, int newval) {
   if (adl_debug)
      printf("(%s) Starting adapterNdx=%d, displayNdx=%d, vcp_feature_code=0x%02x\n",
             __func__, iAdapterIndex, iDisplayIndex, vcp_feature_code );

   unsigned char ddc_cmd_bytes[] = {
      0x6e,              // address 0x37, shifted left 1 bit
      0x51,              // source address
      0x02 | 0x04,       // number of DDC data bytes, with high bit set
      0x03,              // DDC Set Feature Command
      vcp_feature_code,  //
      newval & 0xff00 >> 8,
      newval & 0xff,
      0x00,              // checksum, to be set
   };
   ddc_cmd_bytes[7] = ddc_checksum(ddc_cmd_bytes, 7, false);    // calculate DDC checksum on all bytes

   int sendCt = sizeof(ddc_cmd_bytes);
   assert (sendCt == 8);

   Base_Status_ADL rc = adl_ddc_write_only(iAdapterIndex, iDisplayIndex, ddc_cmd_bytes, sendCt);

   sleep_millis_with_trace(DDC_TIMEOUT_MILLIS_POST_SETVCP_WRITE, __func__, "after adl_DDC_write_only");

   if (adl_debug)
      printf("(%s) Returning %d  \n", __func__, rc );
   return rc;
}
