/* file_util.c
 *
 * Created on: Dec 6, 2015
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
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "util/report_util.h"
#include "util/string_util.h"

#include "util/file_util.h"


int file_getlines(const char * fn,  GPtrArray* line_array) {
   bool debug = false;
   int rc = 0;
   if (debug)
      printf("(%s) Starting. fn=%s  \n", __func__, fn );
   FILE * fp = fopen(fn, "r");
   if (!fp) {
      int errsv = errno;
      rc = -errno;
      fprintf(stderr, "Error opening file %s: %s\n", fn, strerror(errsv));
   }
   else {
      char * line = NULL;
      size_t len = 0;
      ssize_t read;
      // int     ct;
      // char    s0[32], s1[257], s2[16];
      // char *  head;
      // char *  rest;
      int     linectr = 0;

      while ((read = getline(&line, &len, fp)) != -1) {
         linectr++;
         char * s = strdup(line);
         g_ptr_array_add(line_array, s);
         // printf("Retrieved line of length %zu :\n", read);
         // printf("%s", line);
      }
      rc = linectr;
   }
   if (debug)
      printf("(%s) Done. returning: %d\n", __func__, rc);
   return rc;
}


char * read_one_line_file(char * fn, bool verbose) {
   FILE * fp = fopen(fn, "r");
   char * single_line = NULL;
   if (!fp) {
      if (verbose)
         fprintf(stderr, "Error opening %s: %s\n", fn, strerror(errno));
   }
   else {
      size_t len = 0;
      ssize_t read;
      // just one line:
      read = getline(&single_line, &len, fp);
      if (read == -1) {
         if (verbose)
           printf("Nothing to read from %s\n", fn);
      }
      else {
         if (strlen(single_line) > 0)
            single_line[strlen(single_line)-1] = '\0';
         // printf("\n%s", single_line);     // single_line has trailing \n
      }
   }
   return single_line;
}


int rpt_file_contents(const char * fn, int depth) {
   GPtrArray * line_array = g_ptr_array_new();
   int rc = file_getlines(fn, line_array);
   if (rc > 0) {
      int ndx = 0;
      for (; ndx < line_array->len; ndx++) {
         char * curline = g_ptr_array_index(line_array, ndx);
         rtrim_in_place(curline);     // strip trailing newline
         rpt_title(curline, depth);
      }
   }
   return rc;
}


bool regular_file_exists(const char * fqfn) {
   bool result = false;
   struct stat stat_buf;
   int rc = stat(fqfn, &stat_buf);
   if (rc == 0) {
      result = S_ISREG(stat_buf.st_mode);
   }
   return result;
}


bool directory_exists(const char * fqfn) {
   bool result = false;
   struct stat stat_buf;
   int rc = stat(fqfn, &stat_buf);
   if (rc == 0) {
      result = S_ISDIR(stat_buf.st_mode);
   }
   return result;
}
