/* ddcutil_status_codes.h
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** \file ddcutil_status_codes.h
 * This file defines the DDC specific status codes that can be returned in #DDCA_Status.
 * In addition to these codes, #DDCA_Status can contain:
 *   - negative Linux errno values
 *   - modulated ADL status codes
 *     (i.e. ADL status codes with a constant added so as not to overlap with Linux errno values)
 *
 * Because the DDC specific status codes are merged with the Linux and ADL
 * status codes (which are #defines), they are specified as #defines rather than enum values.
 */

#ifndef DDCUTIL_STATUS_CODES_H_
#define DDCUTIL_STATUS_CODES_H_

#define RCRANGE_DDC_START  3000

#define DDCRC_OK                     0

#define DDCRC_PACKET_SIZE            (-RCRANGE_DDC_START-1)     // deprecated, use DDCRC_DDC_DATA
#define DDCRC_RESPONSE_ENVELOPE      (-RCRANGE_DDC_START-2)     // deprecated, use DDCRC_DDC_DATA
#define DDCRC_CHECKSUM               (-RCRANGE_DDC_START-3)     // deprecated, use DDCRC_DDC_DATA
#define DDCRC_INVALID_DATA           (-RCRANGE_DDC_START-4)     // deprecated, use DDCRC_DDC_DATA or DDCL_ARG
#define DDCRC_RESPONSE_TYPE          (-RCRANGE_DDC_START-5)     // deprecated, use DDCRC_DDC_DATA
#define DDCRC_NULL_RESPONSE          (-RCRANGE_DDC_START-6)
#define DDCRC_MULTI_PART_READ_FRAGMENT  (-RCRANGE_DDC_START-7)
#define DDCRC_ALL_TRIES_ZERO         (-RCRANGE_DDC_START-8)     // packet data entirely 0  // not used TODO eliminate
#define DDCRC_DOUBLE_BYTE            (-RCRANGE_DDC_START-9)     // duplicated byte in packet // deprecated, use DDCRC_DDC_DATA
#define DDCRC_REPORTED_UNSUPPORTED   (-RCRANGE_DDC_START-10)    // DDC reply says unsupported
#define DDCRC_READ_ALL_ZERO          (-(RCRANGE_DDC_START+11) )
#define DDCRC_BAD_BYTECT             (-(RCRANGE_DDC_START+12) ) // deprecated, use DDCRC_DDC_DATA
#define DDCRC_READ_EQUALS_WRITE      (-(RCRANGE_DDC_START+13) ) // deprecated, use DDCRC_DDC_DATA
#define DDCRC_INVALID_MODE           (-(RCRANGE_DDC_START+14) ) // deprecated
#define DDCRC_RETRIES                (-(RCRANGE_DDC_START+15) ) // too many retries
#define DDCRC_EDID                   (-(RCRANGE_DDC_START+16) ) // deprecated, use DDCRC_READ_EDID or DDCRC_INVALID_EDID
#define DDCRC_DETERMINED_UNSUPPORTED (-(RCRANGE_DDC_START+17) ) // facility determined to be unsupported

#define DDCRC_ARG                     (-(RCRANGE_DDC_START+18) ) // illegal argument
#define DDCRC_INVALID_OPERATION       (-(RCRANGE_DDC_START+19) ) // e.g. writing a r/o feature
#define DDCRC_UNIMPLEMENTED           (-(RCRANGE_DDC_START+20) ) // unimplemented service
#define DDCRC_UNINITIALIZED           (-(RCRANGE_DDC_START+21) ) // library not initialized
#define DDCRC_UNKNOWN_FEATURE         (-(RCRANGE_DDC_START+22) ) // feature not in feature table
#define DDCRC_INTERPRETATION_FAILED  (-(RCRANGE_DDC_START+23) ) // value format failed
#define DDCRC_MULTI_FEATURE_ERROR    (-(RCRANGE_DDC_START+24) ) // an error occurred on a multi-feature request
#define DDCRC_INVALID_DISPLAY        (-(RCRANGE_DDC_START+25) ) // monitor not found, can't open, no DDC support, etc
#define DDCRC_INTERNAL_ERROR          (-(RCRANGE_DDC_START+26) ) // error that triggers program failure
#define DDCRC_OTHER                   (-(RCRANGE_DDC_START+27) ) // other error (for use during development)
#define DDCRC_VERIFY                 (-(RCRANGE_DDC_START+28) ) // read after VCP write failed or wrong value
#define DDCRC_NOT_FOUND              (-(RCRANGE_DDC_START+29) ) // generic not found
#define DDCRC_ALL_RESPONSES_NULL     (-(RCRANGE_DDC_START+30) ) // all responses are DDC Null Message
#define DDCRC_DDC_DATA               (-(RCRANGE_DDC_START+31) ) // replaces DDCRC_PACKET_SIZE, etc.
#define DDCRC_READ_EDID              (-(RCRANGE_DDC_START+32) ) // error reading EDID
#define DDCRC_INVALID_EDID           (-(RCRANGE_DDC_START+33) ) // error parsing EDID


// TODO: consider replacing DDCRC_EDID by more generic DDCRC_BAD_DATA, could be used for e.g. invalid capabilities string
// what about DDCRC_INVALID_DATA?
// maybe most of DDCRC_... become DDCRC_I2C...

#endif /* DDCUTIL_STATUS_CODES_H_ */
