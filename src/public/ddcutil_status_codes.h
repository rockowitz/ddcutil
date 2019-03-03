/** @file ddcutil_status_codes.h
 *
 * This file defines the DDC specific status codes that can be returned in #DDCA_Status.
 * In addition to these codes, #DDCA_Status can contain:
 *   - negative Linux errno values
 *   - modulated ADL status codes
 *     (i.e. ADL status codes with a constant added or subtracted so as not to overlap
 *      with Linux errno values)
 *
 * Because the DDC specific status codes are merged with the Linux and ADL status codes
 * (which are \#defines), they are specified as \#defines rather than enum values.
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef DDCUTIL_STATUS_CODES_H_
#define DDCUTIL_STATUS_CODES_H_

#define RCRANGE_DDC_START  3000

#define DDCRC_OK                     0                           ///< Success
#define DDCRC_DDC_DATA               (-(RCRANGE_DDC_START+1 ) )  ///< DDC data error
#define DDCRC_NULL_RESPONSE          (-(RCRANGE_DDC_START+2 ) )  //!< DDC Null Response received
#define DDCRC_MULTI_PART_READ_FRAGMENT (-(RCRANGE_DDC_START+3) ) ///< Error in multi-part read fragment
#define DDCRC_ALL_TRIES_ZERO         (-(RCRANGE_DDC_START+4 ) )  ///< packet data entirely 0
#define DDCRC_REPORTED_UNSUPPORTED   (-(RCRANGE_DDC_START+5 ) )  ///< DDC reply says unsupported
#define DDCRC_READ_ALL_ZERO          (-(RCRANGE_DDC_START+6 ) )  ///<
#define DDCRC_RETRIES                (-(RCRANGE_DDC_START+7 ) )  ///< too many retries
#define DDCRC_EDID                   (-(RCRANGE_DDC_START+8 ) )  ///< still in use, use DDCRC_READ_EDID or DDCRC_INVALID_EDID
#define DDCRC_READ_EDID              (-(RCRANGE_DDC_START+9 ) )  ///< error reading EDID
#define DDCRC_INVALID_EDID           (-(RCRANGE_DDC_START+10) )  ///< error parsing EDID
#define DDCRC_ALL_RESPONSES_NULL     (-(RCRANGE_DDC_START+11) )  ///< all responses are DDC Null Message
#define DDCRC_DETERMINED_UNSUPPORTED (-(RCRANGE_DDC_START+12) )  ///< feature determined to be unsupported

#define DDCRC_ARG                    (-(RCRANGE_DDC_START+13) ) ///< illegal argument
#define DDCRC_INVALID_OPERATION      (-(RCRANGE_DDC_START+14) ) ///< e.g. writing a r/o feature
#define DDCRC_UNIMPLEMENTED          (-(RCRANGE_DDC_START+15) ) ///< unimplemented service
#define DDCRC_UNINITIALIZED          (-(RCRANGE_DDC_START+16) ) ///< library not initialized
#define DDCRC_UNKNOWN_FEATURE        (-(RCRANGE_DDC_START+17) ) ///< feature not in feature table
#define DDCRC_INTERPRETATION_FAILED  (-(RCRANGE_DDC_START+18) ) ///< value format failed
#define DDCRC_MULTI_FEATURE_ERROR    (-(RCRANGE_DDC_START+19) ) ///< an error occurred on a multi-feature request
#define DDCRC_INVALID_DISPLAY        (-(RCRANGE_DDC_START+20) ) ///< monitor not found, can't open, no DDC support, etc
#define DDCRC_INTERNAL_ERROR         (-(RCRANGE_DDC_START+21) ) ///< error that triggers program failure
#define DDCRC_OTHER                  (-(RCRANGE_DDC_START+22) ) ///< other error (for use during development)
#define DDCRC_VERIFY                 (-(RCRANGE_DDC_START+23) ) ///< read after VCP write failed or wrong value
#define DDCRC_NOT_FOUND              (-(RCRANGE_DDC_START+24) ) ///< generic not found
#define DDCRC_LOCKED                 (-(RCRANGE_DDC_START+25) ) ///< resource locked
#define DDCRC_ALREADY_OPEN           (-(RCRANGE_DDC_START+26) ) ///< already open in current thread
#define DDCRC_BAD_DATA               (-(RCRANGE_DDC_START+27) ) ///< invalid data

// TODO: consider replacing DDCRC_INVALID_EDID by a more generic DDCRC_BAD_DATA,
//       or DDC_INVALID_DATA, could be used for e.g. invalid capabilities string

#endif /* DDCUTIL_STATUS_CODES_H_ */
