/** \file
 * Error codes internal to **ddcutil**.
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <stdio.h>
/** \endcond */

#include "util/string_util.h"

#include "base/ddc_errno.h"

//
// DDCRC status code descriptions
//

// TODO: Consider modifying EDENTRY generate doxygen comment as well using description field

#define EDENTRY(id,desc) {id, #id, desc}

// DDCRC_DOUBLE_BYTE probably not worth keeping, can only reliably check for
// small subset of DDCRC_PACKET_SIZE, DDCRC_RESPONSE_ENVELOPE, DDCRC_CHECKSUM

static Status_Code_Info ddcrc_info[] = {
      EDENTRY(DDCRC_OK                       , "success"                         ),
      EDENTRY(DDCRC_DDC_DATA                 , "DDC data error"                  ),
      EDENTRY(DDCRC_NULL_RESPONSE            , "received DDC null response"      ),
      EDENTRY(DDCRC_MULTI_PART_READ_FRAGMENT , "error in fragment"               ),
      EDENTRY(DDCRC_ALL_TRIES_ZERO           , "every try response 0x00"          ),    // applies to multi-try exchange
      EDENTRY(DDCRC_REPORTED_UNSUPPORTED     , "DDC reports facility unsupported" ),
      EDENTRY(DDCRC_READ_ALL_ZERO            , "packet contents entirely 0x00"    ),
      EDENTRY(DDCRC_RETRIES                  , "maximum retries exceeded"         ),
      EDENTRY(DDCRC_EDID                     , "invalid EDID"                     ),
      EDENTRY(DDCRC_READ_EDID                , "unable to read EDID"              ),
      EDENTRY(DDCRC_INVALID_EDID             , "unable to parse EDID"             ),
      EDENTRY(DDCRC_ALL_RESPONSES_NULL       , "all tries returned DDC Null Message"),
      EDENTRY(DDCRC_DETERMINED_UNSUPPORTED   , "ddcutil determined that facility unsupported" ),

      // library errors
      EDENTRY(DDCRC_ARG                      , "illegal argument"),
      EDENTRY(DDCRC_INVALID_OPERATION        , "invalid operation"),
      EDENTRY(DDCRC_UNIMPLEMENTED            , "unimplemented"),
      EDENTRY(DDCRC_UNINITIALIZED            , "library uninitialized"),
      EDENTRY(DDCRC_UNKNOWN_FEATURE          , "feature not in feature table"),
      EDENTRY(DDCRC_INTERPRETATION_FAILED    , "feature value interpretation function failed"),
      EDENTRY(DDCRC_MULTI_FEATURE_ERROR      , "at least 1 error occurred on a multi-feature request"),
      EDENTRY(DDCRC_INVALID_DISPLAY          , "invalid display"),
      EDENTRY(DDCRC_INTERNAL_ERROR           , "fatal error condition"),
      EDENTRY(DDCRC_OTHER                    , "other error"),       // for use during development
      EDENTRY(DDCRC_VERIFY                   , "VCP read after write failed"),
      EDENTRY(DDCRC_NOT_FOUND                , "not found"),
      EDENTRY(DDCRC_LOCKED                   , "display locked"),
      EDENTRY(DDCRC_BAD_DATA                 , "invalid data"),

    };
#undef EDENTRY
static int ddcrc_desc_ct = sizeof(ddcrc_info)/sizeof(Status_Code_Info);

/** Returns the #Status_Code_Info struct for a **ddcutil** status code.
 *
 * @param  rc   ddcutil status code
 * @return pointer to #Status_Code_Info, NULL if not found
 *
 * @remark
 * Returns a pointer into a struct compiled into the executable.
 * Do not deallocate.
 * @remark
 * **ddcutil** status codes are always modulated.
 */
Status_Code_Info * ddcrc_find_status_code_info(int rc) {
   Status_Code_Info * result = NULL;
   for (int ndx=0; ndx < ddcrc_desc_ct; ndx++) {
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

/** Certain **ddcutil** status codes (e.g. DDCRC_DETERMINED_UNSUPPORTED)
 *  are "derived" at higher levels from primary **ddcutil** status codes
 *  in lower level routines.  These should be excluded from certain error
 *  counts as otherwise an error would be double counted.
 *
 *  @param gsc status code
 *  @return true/false
 */
bool ddcrc_is_derived_status_code(Public_Status_Code gsc) {
   return (gsc == DDCRC_ALL_TRIES_ZERO         ||
           gsc == DDCRC_RETRIES                ||
           gsc == DDCRC_DETERMINED_UNSUPPORTED
          );
}

/** Certain **ddcutil** status codes, (e.g. DDCRC_REPORTED_UNSUPPORTED)
 *  report states that should not be considered to be DDC protocol errors.
 */
bool ddcrc_is_not_error(Public_Status_Code gsc) {
   return (gsc == DDCRC_REPORTED_UNSUPPORTED);
}


/* Returns a sting description of a **ddcutil** status code that is
 * intended for use in error messages.
 *
 * @param rc  ddcutil status code
 * @return status code description
 *
 * @remark
 * The result is built in an internal buffer.  The contents will be
 * valid until the next call to this function.
 * @remark
 * A generic message is returned if the status code is unrecognized.
 */
char * ddcrc_desc(int rc) {
   static char workbuf[200];
   Status_Code_Info * pdesc = ddcrc_find_status_code_info(rc);
   if (pdesc) {
      snprintf(workbuf, 200,
               "%s(%d): %s", pdesc->name, rc, pdesc->description);
   }
   else {
      snprintf(workbuf, 200, "Unexpected status code %d", rc);
   }
   return workbuf;
}


/** Gets the (unmodulated) ddcutil error number for a symbolic name.
 *
 * @param   error_name   symbolic name, e.g. DDCRC_CHECKSUM
 * @param   p_errnum     where to return error number
 *
 * Returns:         true if found, false if not
 *
 * @remark
 * Since **ddcutil** specific error numbers are always modulated,
 * the return value for this function is always identical to
 * ddc_error_name_to_modulated_number().
 */
bool ddc_error_name_to_number(const char * error_name, Status_DDC * p_errnum) {
   int found = false;
   *p_errnum = 0;
   for (int ndx = 0; ndx < ddcrc_desc_ct; ndx++) {
       if ( streq(ddcrc_info[ndx].name, error_name) ) {
          *p_errnum = ddcrc_info[ndx].code;
          found = true;
          break;
       }
   }
   return found;
}

