/*
 * ddc_errno.c
 *
 *  Created on: Nov 8, 2015
 *      Author: rock
 */

#include <stdio.h>

#include <base/status_code_mgt.h>

#include <base/ddc_errno.h>


// testing

#ifdef NO
const int NEWRC_PACKET_SIZE           = (-RCRANGE_DDC_BASE-1);
const int NEWRC_RESPONSE_ENVELOPE     = (-RCRANGE_DDC_BASE-2);
const int NEWRC_CHECKSUM              = (-RCRANGE_DDC_BASE-3);
const int NEWRC_INVALID_DATA          = (-RCRANGE_DDC_BASE-4);
const int NEWRC_RESPONSE_TYPE         = (-RCRANGE_DDC_BASE-5);
const int NEWRC_NULL_RESPONSE         = (-RCRANGE_DDC_BASE-6);
const int NEWRC_CAPABILITIES_FRAGMENT = (-RCRANGE_DDC_BASE-7);
const int NEWRC_ZEROS                 = (-RCRANGE_DDC_BASE-8);    // packet data entirely 0
const int NEWRC_DOUBLE_BYTE           = (-RCRANGE_DDC_BASE-9);    // duplicated byte in packet
const int NEWRC_UNSUPPORTED           = (-RCRANGE_DDC_BASE-10);   // unsupported facility
const int NEWRC_READ_ALL_ZERO         = (-(RCRANGE_DDC_BASE+11) );
const int NEWRC_BAD_BYTECT            = (-(RCRANGE_DDC_BASE+12) );
const int NEWRC_READ_EQUALS_WRITE     = (-(RCRANGE_DDC_BASE+13) );
const int NEWRC_INVALID_MODE          = (-(RCRANGE_DDC_BASE+14) );
const int NEWRC_RETRIES               = (-(RCRANGE_DDC_BASE+15) ); // too many retries
const int NEWRC_PACKET_ERROR_END      = (-RCRANGE_DDC_BASE-16);   // special end value
#endif

//
// DDCRC status code descriptions
//

#define EDENTRY(id,desc) {id, #id, desc}

static Status_Code_Info ddcrc_desc[] = {
      EDENTRY(DDCRC_OK                    , "success"                       ),
      EDENTRY(DDCRC_PACKET_SIZE           , "packet data field too large"   ),
      EDENTRY(DDCRC_RESPONSE_ENVELOPE     , "invalid source address in reply packet"),
      EDENTRY(DDCRC_CHECKSUM              , "checksum error"                ),
      EDENTRY(DDCRC_RESPONSE_TYPE         , "incorrect response type"       ),
      EDENTRY(DDCRC_INVALID_DATA          , "error parsing data bytes"      ),
      EDENTRY(DDCRC_NULL_RESPONSE         , "received DDC null response"    ),
      EDENTRY(DDCRC_CAPABILITIES_FRAGMENT , "error in fragment"             ),
      EDENTRY(DDCRC_ZEROS                 , "response entirely 0x00"        ),
      EDENTRY(DDCRC_DOUBLE_BYTE           , "duplicated byte in response"   ),
      EDENTRY(DDCRC_UNSUPPORTED           , "unsupported facility"                  ),
      EDENTRY(DDCRC_READ_ALL_ZERO         , "packet contents entirely 0x00"         ),
      EDENTRY(DDCRC_BAD_BYTECT            , "wrong number of bytes in DDC response" ),
      EDENTRY(DDCRC_READ_EQUALS_WRITE     , "response identical to request"         ),
      EDENTRY(DDCRC_INVALID_MODE          , "invalid read or write mode"            ),
      EDENTRY(DDCRC_RETRIES               , "maximum retries exceeded"              ),
      EDENTRY(DDCRC_EDID                  , "invalid EDID"                          ),
    };
#undef EDENTRY
static int ddcrc_desc_ct = sizeof(ddcrc_desc)/sizeof(Status_Code_Info);
// static bool rc_desc_initialized = false;

// #define DDCRC_MEMOIZED_DESCRIPTION_MAX  80
// static char ddcrc_description_buffer[DDCRC_MEMOIZED_DESCRIPTION_MAX];

#ifdef OLD
void initialize_ddcrc_desc() {
   int ndx = 0;
   for (; ndx < ddcrc_desc_ct; ndx++) {
      ddcrc_desc[ndx].memoized_description = malloc(DDCRC_MEMOIZED_DESCRIPTION_MAX
            );
      snprintf(ddcrc_desc[ndx].memoized_description,
               DDCRC_MEMOIZED_DESCRIPTION_MAX,
               "%s (%d) - %s", ddcrc_desc[ndx].name, ddcrc_desc[ndx].code, ddcrc_desc[ndx].description);
   }
   rc_desc_initialized = true;
}
#endif



Status_Code_Info * find_ddcrc_status_code_description(int rc) {
   // if (!rc_desc_initialized)
   //    initialize_ddcrc_desc();

   Status_Code_Info * result = NULL;

   int ndx;
   for (ndx=0; ndx < ddcrc_desc_ct; ndx++) {
       if (rc == ddcrc_desc[ndx].code) {
          result = &ddcrc_desc[ndx];
          break;
       }
   }
   return result;
}


static char workbuf[200];

/*
 *
 *
 * Caller should NOT free memory
 */
char * ddcrc_description(int rc) {
   char * result = NULL;
   Status_Code_Info * pdesc = find_ddcrc_status_code_description(rc);
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

