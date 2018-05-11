/* demo_redirection.c
 *
 * Sample program illustrating the use of libddcutil's functions for
 * redirecting and capturing program output.
 *
 * <copyright>
 * Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
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


void capture_output_using_convenience_functions() {
   printf("\nCapturing output using API convenience functions...\n");

   ddca_start_capture(DDCA_CAPTURE_NOOPTS);
   int logical_indentation_depth = 1;
   ddca_report_displays(false, logical_indentation_depth);
   char * output = ddca_end_capture();
   printf("Captured output:\n%s\n", output);
   free(output);
}


void capture_output_using_basic_functions() {
   printf("\nCapturing output to in core buffer using basic API functions..\n");

   size_t size;
   char * ptr;
   // Alternatively, use fmemopen() to use a pre-allocated in-memory buffer
   // or fopen() to open a file.
   FILE * f = open_memstream(&ptr, &size);
   if (!f) {
      perror("Error opening file ");
      return;
   }

   ddca_set_fout(f);
   int logical_indentation_depth = 1;
   ddca_report_displays(false, logical_indentation_depth);
   // Ensure output actually written to FILE:
   int rc = fflush(f);
   if (rc < 0) {
      perror("fflush() failed");
      return;
   }

   // size does not include trailing null appended by fmemopen()
   printf("Size after writing to buffer: %zd\n", size);
   ddca_set_fout_to_default();
   // must copy data before closing buffer
   char * result = strdup(ptr);
   rc = fclose(f);
   if (rc < 0) {
      perror("Error closing in core buffer");
      free(result);
      return;
   }

   printf("Output:\n");
   printf("%s\n", result);
   free(result);
}


int main(int argc, char** argv) {
   capture_output_using_convenience_functions();
   capture_output_using_basic_functions();
}
