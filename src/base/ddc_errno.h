/*  ddc_errno.h
 *
 *  Created on: Nov 8, 2015
 *      Author: rock
 *
 *  Error codes internal to the application, which are
 *  primarily DDC related.
 */

#ifndef APP_ERRNO_H_
#define APP_ERRNO_H_

#include <base/status_code_mgt.h>


// Why not use #define:
// - Eclipse global name change doesn't work well
//
// Why not use enum:
//  Full set of status codes is the union of modulate(errno), modulated(adl_error_number), app specific error numbers
//
// Disadvantage:
// - can't use as case values for switch
//
// Reason for using defines:
//  errno.h  values are defines
//  adl error values are defines
//


// Status codes created by this application
// (as opposed to Linux ERRNO, ADL)
// These generally indicate a DDC protocol problem

#define DDCRC_OK                     0

#define DDCRC_PACKET_SIZE            (-RCRANGE_DDC_START-1)
#define DDCRC_RESPONSE_ENVELOPE      (-RCRANGE_DDC_START-2)
#define DDCRC_CHECKSUM               (-RCRANGE_DDC_START-3)
#define DDCRC_INVALID_DATA           (-RCRANGE_DDC_START-4)
#define DDCRC_RESPONSE_TYPE          (-RCRANGE_DDC_START-5)
#define DDCRC_NULL_RESPONSE          (-RCRANGE_DDC_START-6)
#define DDCRC_CAPABILITIES_FRAGMENT  (-RCRANGE_DDC_START-7)
#define DDCRC_ALL_TRIES_ZERO         (-RCRANGE_DDC_START-8)    // packet data entirely 0  // not used TODO eliminate
#define DDCRC_DOUBLE_BYTE            (-RCRANGE_DDC_START-9)    // duplicated byte in packet
#define DDCRC_REPORTED_UNSUPPORTED   (-RCRANGE_DDC_START-10)   // DDC reply says unsupported
#define DDCRC_READ_ALL_ZERO          (-(RCRANGE_DDC_START+11) )
#define DDCRC_BAD_BYTECT             (-(RCRANGE_DDC_START+12) )
#define DDCRC_READ_EQUALS_WRITE      (-(RCRANGE_DDC_START+13) )
#define DDCRC_INVALID_MODE           (-(RCRANGE_DDC_START+14) )
#define DDCRC_RETRIES                (-(RCRANGE_DDC_START+15) ) // too many retries
#define DDCRC_EDID                   (-(RCRANGE_DDC_START+16) )  // invalid EDID
#define DDCRC_DETERMINED_UNSUPPORTED (-(RCRANGE_DDC_START+17) ) // facility determined to be unsupported
// never used
// #define DDCRC_PACKET_ERROR_END      (-RCRANGE_DDC_START-16)   // special end value


Status_Code_Info * ddcrc_find_status_code_info(int rc);

// Returns status code description:
char * ddcrc_desc(int rc);   // must be freed after use

bool ddcrc_is_derived_status_code(Global_Status_Code gsc);

bool ddcrc_is_not_error(Global_Status_Code gsc);



#endif /* APP_ERRNO_H_ */
