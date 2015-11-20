/*
 * ddc_errno.c
 *
 *  Created on: Nov 8, 2015
 *      Author: rock
 */

#include <stdio.h>

#include <base/status_code_mgt.h>

#include <base/ddc_errno.h>


//
// DDCRC status code descriptions
//

#define EDENTRY(id,desc) {id, #id, desc}

static Status_Code_Info ddcrc_info[] = {
      EDENTRY(DDCRC_OK                    , "success"                       ),
      EDENTRY(DDCRC_PACKET_SIZE           , "packet data field too large"   ),
      EDENTRY(DDCRC_RESPONSE_ENVELOPE     , "invalid source address in reply packet"),
      EDENTRY(DDCRC_CHECKSUM              , "checksum error"                ),
      EDENTRY(DDCRC_RESPONSE_TYPE         , "incorrect response type"       ),
      EDENTRY(DDCRC_INVALID_DATA          , "error parsing data bytes"      ),
      EDENTRY(DDCRC_NULL_RESPONSE         , "received DDC null response"    ),
      EDENTRY(DDCRC_CAPABILITIES_FRAGMENT , "error in fragment"             ),
      EDENTRY(DDCRC_ALL_TRIES_ZERO        , "every try response 0x00"        ),    // applies to multi-try exchange
      EDENTRY(DDCRC_DOUBLE_BYTE           , "duplicated byte in response"   ),
      EDENTRY(DDCRC_REPORTED_UNSUPPORTED  , "DDC reports facility unsupported"      ),
      EDENTRY(DDCRC_READ_ALL_ZERO         , "packet contents entirely 0x00"         ),
      EDENTRY(DDCRC_BAD_BYTECT            , "wrong number of bytes in DDC response" ),
      EDENTRY(DDCRC_READ_EQUALS_WRITE     , "response identical to request"         ),
      EDENTRY(DDCRC_INVALID_MODE          , "invalid read or write mode"            ),
      EDENTRY(DDCRC_RETRIES               , "maximum retries exceeded"              ),
      EDENTRY(DDCRC_EDID                  , "invalid EDID"                          ),
      EDENTRY(DDCRC_DETERMINED_UNSUPPORTED , "ddctool determined that facility unsupported" ),
    };
#undef EDENTRY
static int ddcrc_desc_ct = sizeof(ddcrc_info)/sizeof(Status_Code_Info);


Status_Code_Info * ddcrc_find_status_code_info(int rc) {
   Status_Code_Info * result = NULL;

   int ndx;
   for (ndx=0; ndx < ddcrc_desc_ct; ndx++) {
       if (rc == ddcrc_info[ndx].code) {
          result = &ddcrc_info[ndx];
          break;
       }
   }
   return result;
}

/* Status code classification

 DDCRC_NULL_RESPONSE
 DDCRC_ALL_TRIES_ZERO
 DDCRC_REPORTED_UNSUPPORTED
 DDCRC_DETERMINED_UNSUPPORTED

 DDCRC_REPORTED_UNSUPPORTED is a primary error, but reports a state, not really an error

 Derived codes are set after function has examined a primary code
 Do not count as DDC errors.
 Derived:
 DDCRC_ALL_TRIES_ZERO
 DDCRC_RETRIES
 DDCRC_DETERMINED_UNSUPPORTED

 DDCRC NULL_RESPONSE is ambiguous
   can be expected (DDC detection)
   no answer to give, e.g. because not ready, not expected (protocol error)
 but also is used by some monitors to indicate invalid request (e.g. unsupported VCP code)

 All others indicate real, primary errors


 Issues:
 - DDCRC_REPORTED_UNSUPPORTED should not be a fatal failure in try_stats,
   it is a successful try, it's just that the response is "unsupported"
 *
 */

bool ddcrc_is_derived_status_code(Global_Status_Code gsc) {
   return (gsc == DDCRC_ALL_TRIES_ZERO         ||
           gsc == DDCRC_RETRIES                ||
           gsc == DDCRC_DETERMINED_UNSUPPORTED
          );
}

bool ddcrc_is_not_error(Global_Status_Code gsc) {
   return (gsc == DDCRC_REPORTED_UNSUPPORTED);
}

static char workbuf[200];

/*
 *
 *
 * Caller should NOT free memory
 */
char * ddcrc_desc(int rc) {
   char * result = NULL;
   Status_Code_Info * pdesc = ddcrc_find_status_code_info(rc);
   if (pdesc) {
      snprintf(workbuf, 200,
               "%s(%d): %s", pdesc->name, rc, pdesc->description);
   }
   else {
      snprintf(workbuf, 200, "Unexpected status code %d", rc);
   }
   result = workbuf;
   return result;
}

