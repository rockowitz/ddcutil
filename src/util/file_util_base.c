/** \file file_util_base.c
 *  Core file utility functions.
 *  Factored out of file_util.h so that includes in directory util form
 *  a directed graph.
 */

// Copyright (C) 2018-2022 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/** \endcond */

#include "data_structures.h"
#include "string_util.h"

#include "file_util_base.h"


/** Reads the lines of a text file into a GPtrArray.
 *
 *  @param  fn          file name
 *  @param  line_array  pointer to GPtrArray of strings where lines will be saved
 *  @param  verbose     if true, write message to stderr if unable to open file or other error
 *
 *  @retval >=0:  number of lines added to line_array
 *  @retval <0    -errno from fopen() or getline()
 *
 *  The caller is responsible for freeing the lines added to line_array.
 *
 *  Strings are appended to #line_array.  It is not cleared at start of
 *  function execution.
 */
int file_getlines(
      const char * fn,
      GPtrArray*   line_array,
      bool         verbose)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. fn=%s  \n", __func__, fn );

   int rc = 0;
   FILE * fp = fopen(fn, "r");
   if (!fp) {
      int errsv = errno;
      rc = -errno;
      if (verbose || debug)
         fprintf(stderr, "Error opening file %s: %s\n", fn, strerror(errsv));
   }
   else {
      // if line == NULL && len == 0, then getline allocates buffer for line
      char * line = NULL;
      size_t len = 0;
      int     linectr = 0;
      errno = 0;
      // int     getline_rc = 0;

      while ( getline(&line, &len, fp) >= 0) {
         linectr++;
         rtrim_in_place(line);     // strip trailing newline
         g_ptr_array_add(line_array, line);   // line will be freed when line_array is freed
         if (debug) {
            printf("(%s) Retrieved line of length %zu, trimmed length %zu: %s\n",
                      __func__, len, strlen(line), line);
         }
         line = NULL;  // reset for next getline() call
         len  = 0;
      }
      // From the getline() man page:
      //    If *lineptr is set to NULL and *n is set 0 before the call,
      //    then getline() will allocate a buffer for storing the line.
      //    This buffer should be freed by the user program even if getline() failed.
      free(line);

      // assert(getline_rc < 0);
      if (errno != 0) {   // was it an error or eof?file_getlines
         rc = -errno;
         if (verbose || debug)
            fprintf(stderr, "Error reading file %s: %s\n", fn, strerror(-rc));
         }
      else
         rc = linectr;

      fclose(fp);
   }

   if (debug)
      printf("(%s) Done. returning: %d\n", __func__, rc);
   return rc;
}

