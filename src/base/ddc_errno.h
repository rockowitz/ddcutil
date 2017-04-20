/* ddc_errno.h
 *
 * Error codes internal to the application, which are
 * primarily ddcutil related.
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
 * Error codes internal to **ddcutil**.
 */


#ifndef DDC_ERRNO_H_
#define DDC_ERRNO_H_

#include "base/status_code_mgt.h"

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
#define DDCRC_MULTI_PART_READ_FRAGMENT  (-RCRANGE_DDC_START-7)
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

#define DDCL_ARG                     (-(RCRANGE_DDC_START+18) ) // illegal argument
#define DDCL_INVALID_OPERATION       (-(RCRANGE_DDC_START+19) ) // e.g. writing a r/o feature
#define DDCL_UNIMPLEMENTED           (-(RCRANGE_DDC_START+20) ) // unimplemented service
#define DDCL_UNINITIALIZED           (-(RCRANGE_DDC_START+21) ) // library not initialized
#define DDCL_UNKNOWN_FEATURE         (-(RCRANGE_DDC_START+22) ) // feature not in feature table
#define DDCRC_INTERPRETATION_FAILED  (-(RCRANGE_DDC_START+23) ) // value format failed
#define DDCRC_MULTI_FEATURE_ERROR    (-(RCRANGE_DDC_START+24) ) // an error occurred on a multi-feature request
#define DDCRC_INVALID_DISPLAY        (-(RCRANGE_DDC_START+25) ) // monitor not found, can't open, no DDC support, etc
#define DDCL_INTERNAL_ERROR          (-(RCRANGE_DDC_START+26) ) // error that triggers program failure
#define DDCL_OTHER                   (-(RCRANGE_DDC_START+27) ) // other error (for use during development)
#define DDCRC_VERIFY                 (-(RCRANGE_DDC_START+28) ) // read after VCP write failed or wrong value
#define DDCRC_NOT_FOUND              (-(RCRANGE_DDC_START+29) ) // generic not found


// TODO: consider replacing DDCRC_EDID by more generic DDCRC_BAD_DATA, could be used for e.g. invalid capabilities string
// what about DDCRC_INVALID_DATA?
// maybe most of DDCRC_... become DDCRC_I2C...


// never used
// #define DDCRC_PACKET_ERROR_END      (-RCRANGE_DDC_START-16)   // special end value


Status_Code_Info * ddcrc_find_status_code_info(int rc);

bool ddc_error_name_to_number(const char * errno_name, Status_DDC * perrno);
// bool ddc_error_name_to_modulated_number(const char * errno_name, Global_Status_Code * p_error_number);

// Returns status code description:
char * ddcrc_desc(int rc);   // must be freed after use

bool ddcrc_is_derived_status_code(Public_Status_Code gsc);

bool ddcrc_is_not_error(Public_Status_Code gsc);

#endif /* APP_ERRNO_H_ */
