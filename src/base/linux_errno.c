/* linux_errno.c
 *
 * Created on: Nov 4, 2015
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <base/status_code_mgt.h>
#include <base/linux_errno.h>



// To be considered:  using libexplain.


Status_Code_Info * find_errno_description(int errnum);

// n. called from main before command line parsed, trace control not yet established
void init_linux_errno() {
   register_retcode_desc_finder(
              RR_ERRNO,
              get_negative_errno_info,
              false);                // finder_arg_is_modulated
}


//
// Error handling
//

//
// Known system error numbers
//

#define EDENTRY(id,desc) {id, #id, NULL}

static Status_Code_Info errno_desc[] = {
      EDENTRY(0,        "success"),
      EDENTRY(EPERM,    "Operation not permitted"),
      EDENTRY(ENOENT,   "No such file or directory"),
      EDENTRY(ESRCH,    "No such process"),     //  3
      EDENTRY(EINTR,    "Interrupted system call"),             //  4
      EDENTRY(EIO,      "I/O error"),                           //  5
      EDENTRY(ENXIO,    "No such device or address"),           // 6
      EDENTRY(E2BIG,    "Argument list too long"),
      EDENTRY(ENOEXEC,  "Exec format error"),
      EDENTRY(EBADF,    "Bad file number"),                      //  9
      EDENTRY(ECHILD,   "No child processes"),
      EDENTRY(EAGAIN,   "Try again"),
      EDENTRY(ENOMEM,   "Out of memory"),
      EDENTRY(EACCES,   "Permission denied"),                    // 13
      EDENTRY(EFAULT,   "Bad address"),                          // 14
      EDENTRY(ENOTBLK,  "Block device required"),
      EDENTRY(EBUSY,    "Device or resource busy"),
      EDENTRY(EEXIST,   "File exists"),
      EDENTRY(EXDEV,    "Cross-device link"),
      EDENTRY(ENODEV,   "No such device"),
      EDENTRY(ENOTDIR,  "Not a directory"),
      EDENTRY(EISDIR,   "Is a directory"),
      EDENTRY(EINVAL,   "Invalid argument"),                      // 22
      EDENTRY(ENFILE,   "File table overflow"),
      EDENTRY(EMFILE,   "Too many open files"),
      EDENTRY(ENOTTY,   "Not a typewriter"),
      EDENTRY(ETXTBSY,  "Text file busy"),
      EDENTRY(EFBIG,    "File too large"),
      EDENTRY(ENOSPC,   "No space left on device"),
      EDENTRY(ESPIPE,   "Illegal seek"),
      EDENTRY(EROFS,    "Read-only file system"),
      EDENTRY(EMLINK,   "Too many links"),                       // 31
      EDENTRY(EPIPE,    "Broken pipe"),                          // 32
      EDENTRY(EDOM,     "Math argument out of domain of func"),  // 33
      EDENTRY(ERANGE,   "Math result not representable"),        // 34
      // break in seq
      EDENTRY(EPROTO,   "Protocol error"),                       // 71
};
#undef EDENTRY
static const int errno_desc_ct = sizeof(errno_desc)/sizeof(Status_Code_Info);


#define WORKBUF_SIZE 300
static char workbuf[WORKBUF_SIZE];
static char dummy_errno_description[WORKBUF_SIZE];
static Status_Code_Info dummy_errno_desc;


/* Simple call to get a description string for a Linux errno value.
 *
 * For use in specifically reporting an unmodulated Linux error number.
 *
 * Arguments:
 *    error_number  system errno value
 *
 * Returns:
 *    string describing the error.
 *
 * The string returned is valid until the next call to this function.
 */

char * linux_errno_desc(int error_number) {
   Status_Code_Info * pdesc = find_errno_description(error_number);
   if (pdesc) {
      snprintf(workbuf, WORKBUF_SIZE, "%s(%d): %s",
               pdesc->name, error_number, pdesc->description);
   }
   else {
      snprintf(workbuf, WORKBUF_SIZE, "%d: %s",
               error_number, strerror(error_number));
   }
   return workbuf;
}


char * linux_errno_name(int error_number) {
   Status_Code_Info * pdesc = find_errno_description(error_number);
   return pdesc->name;
}



/* Returns the Status_Code_Info record for the specified error number
 *
 * Arguments:
 *   errnum    linux error number
 *
 * Returns:
 *   Status_Code_Description record, NULL if not found
 *
 * If the description field of the Status_Code_Info struct is NULL, it is set
 * by calling strerror()
 */
Status_Code_Info * find_errno_description(int errnum) {
   Status_Code_Info * result = NULL;
   int ndx;
   for (ndx=0; ndx < errno_desc_ct; ndx++) {
       if (errnum == errno_desc[ndx].code) {
          result = &errno_desc[ndx];
          break;
       }
   }
   if (result && !result->description) {
      char * desc = strerror(errnum);
      result->description  = malloc(strlen(desc)+1);
      strcpy(result->description, desc);
   }
   return result;
}


// n. returned value is valid only until next call
Status_Code_Info * create_dynamic_errno_info(int errnum) {
   Status_Code_Info * result = &dummy_errno_desc;
   result->code = errnum;
   result->name = NULL;

   // would be simpler to use strerror_r(), but the return value of
   // that function depends on compiler switches.
   char * s = strerror(errnum);  // generates an unknown code message for unrecognized errnum
   int sz = sizeof(dummy_errno_description);
   strncpy(dummy_errno_description, s, sz);
   dummy_errno_description[sz-1] = '\0';    // ensure trailing null in case strncpy truncated
   result->description = dummy_errno_description;

   return result;
}



Status_Code_Info * get_errno_info(int errnum) {
   Status_Code_Info * result = find_errno_description(errnum);
   return result;
}


Status_Code_Info * get_negative_errno_info(int errnum) {
   return get_errno_info(-errnum);
}

