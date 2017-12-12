/* demo_global_settings.c
 *
 * Sample program illustrating the use of libddcutil's functions for
 * build information and global settings management.
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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "public/ddcutil_c_api.h"


#define FUNCTION_ERRMSG(function_name,status_code) \
   printf("(%s) %s() returned %d (%s): %s\n",      \
          __func__, function_name, status_code,    \
          ddca_rc_name(status_code),      \
          ddca_rc_desc(status_code))


void demo_build_information() {
   printf("\nProbe static build information...\n");

   // Get the ddcutil version as a string in the form "major.minor.micro".
   printf("   ddcutil version by ddca_ddcutil_version_string(): %s\n", ddca_ddcutil_version_string() );

   // Get the ddcutil version as a struct of integers
   DDCA_Ddcutil_Version_Spec vspec = ddca_ddcutil_version();
   printf("   ddcutil version by ddca_ddcutil_version():  %d.%d.%d\n", vspec.major, vspec.minor, vspec.micro);

   // Get build options
   uint8_t build_options = ddca_get_build_options();
   printf("   Built with ADL support:        %s\n", (build_options & DDCA_BUILT_WITH_ADL)     ? "yes" : "no");
   printf("   Built with USB support:        %s\n", (build_options & DDCA_BUILT_WITH_USB)     ? "yes" : "no");
   printf("   Built with failure simulation: %s\n", (build_options & DDCA_BUILT_WITH_FAILSIM) ? "yes" : "no");
}


void demo_retry_management() {

  printf("\nExercise retry management functions...\n");

  int rc = 0;

  // The maximum retry number that can be specified on ddca_set_max_tries().
  // Any larger number will case the call to fail.
  int max_max_tries = ddca_get_max_max_tries();
  printf("   Maximum try count that can be set: %d\n", max_max_tries);

  // There are separate maximum tries for 3 different types of I2C operations.
  // - DDCA_WRITE_READ_TRIES
  //   The is the most common operation, the host performs a write on the
  //   I2C bus requesting information from the display, which prepares a response.
  //   The subsequent write reads the response from the I2C bus.
  // - DDCA_WRITE_ONLY_TRIES
  //   Used to set a VCP feature value.
  // - DDCA_MULTI_PART_TRIES
  //   Some operations (capabilities, table read/write) transfer more data
  //   than can be transmitted in a single I2C request.   The operation is
  //   broken up into multiple write/read or write-only operations.  This
  //   count controls the number of times the aggregate operation can be
  //   retried.

  // rc = ddca_set_max_tries(DDCA_WRITE_READ_TRIES, 15);
  // printf("(%s) ddca_set_max_tries(DDCA_WRITE_READ_TRIES,15) returned: %d (%s)\n",
  //        __func__, rc, ddca_status_code_name(rc) )

  printf("   Get the current max try settings:\n");
  printf("      max write only tries: %d\n", ddca_get_max_tries(DDCA_WRITE_ONLY_TRIES));
  printf("      max write read tries: %d\n", ddca_get_max_tries(DDCA_WRITE_READ_TRIES));
  printf("      max multi part tries: %d\n", ddca_get_max_tries(DDCA_MULTI_PART_TRIES));

  // The following calls exercise the DDCA_WRITE_READ_TRIES setting.
  // Invocations for DDCA_WRITE_ONLY and DDCA_MULTI_PART_TRIES are similar.

  printf("   Calling ddca_set_max_tries() with a retry count that's too large...\n");
  int badct = max_max_tries + 1;
  rc = ddca_set_max_tries(DDCA_WRITE_READ_TRIES, badct);
  assert(rc == -EINVAL);
  printf("      ddca_set_max_tries(DDCA_WRITE_READ_TRIES, %d) returned: %d (%s)\n",
         badct, rc, ddca_rc_name(rc) );

  printf("   Setting the count to exactly ddca_get_max_max_tries() works...\n");
  rc = ddca_set_max_tries(DDCA_WRITE_READ_TRIES, max_max_tries);
  printf("      ddca_set_max_tries(DDCA_WRITE_READ_TRIES, %d) returned: %d (%s)\n",
         max_max_tries, rc, ddca_rc_name(rc) );

}


#ifdef OBSOLETE

// Register an abort function.
// If libddcutil encounters an unexpected, unrecoverable error, it will
// normally exit, causing the calling program to fail.  If the caller registers an
// abort function, that function will be called instead.
void handle_library_abort() {
   // For aborting out of shared library
   static jmp_buf abort_buf;
   int jmprc = setjmp(abort_buf);
   if (jmprc) {
      DDCA_Global_Failure_Information * finfo = ddca_get_global_failure_information();
      if (finfo)
         fprintf(stderr, "(%s) Error %d (%s) in function %s at line %d in file %s\n",
                         __func__, finfo->status, ddca_rc_name(finfo->status), finfo->funcname, finfo->lineno, finfo->fn);
      fprintf(stderr, "(%s) Aborting. Internal status code = %d\n", __func__, jmprc);
      exit(EXIT_FAILURE);
   }
   ddca_register_jmp_buf(&abort_buf);
}

#endif


int main(int argc, char** argv) {
   // printf("\n(%s) Starting.\n", __func__);

   // Query library build settings.
   demo_build_information();

#ifdef OBSOLETE
   // Catches libddcutil failures that would otherwise cause program abort
   handle_library_abort();
#endif

   // Retry management
   demo_retry_management();
}
