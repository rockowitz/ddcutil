/* file_util.c
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "util/report_util.h"
#include "util/string_util.h"

#include "util/file_util.h"


/* Reads the lines of a text file into a GPtrArray.
 *
 * Arguments:
 *    fn          file name
 *    line_array  pointer to GPtrArray where lines will be saved
 *    verbose     if true, write message to stderr if unable to open file
 *                or other error
 *
 *  Returns:
 *    if >=0:  number of lines added to line_array
 *    <0       -errno
 *
 *  The caller is responsible for freeing the lines added to line_array.
 */
int file_getlines(const char * fn,  GPtrArray* line_array, bool verbose) {
   bool debug = false;
   int rc = 0;
   if (debug)
      printf("(%s) Starting. fn=%s  \n", __func__, fn );
   FILE * fp = fopen(fn, "r");
   if (!fp) {
      int errsv = errno;
      rc = -errno;
      if (verbose)
         fprintf(stderr, "Error opening file %s: %s\n", fn, strerror(errsv));
   }
   else {
      // if line == NULL && len == 0, then getline allocates buffer for line
      char * line = NULL;
      size_t len = 0;
      ssize_t read;
      // int     ct;
      // char    s0[32], s1[257], s2[16];
      // char *  head;
      // char *  rest;
      int     linectr = 0;
      errno = 0;
      while ((read = getline(&line, &len, fp)) != -1) {
         linectr++;
         g_ptr_array_add(line_array, line);
         // printf("Retrieved line of length %zu :\n", read);
         // printf("%s", line);
         line = NULL;
         len  = 0;
      }
      if (errno != 0)  {   // getline error?
         rc = -errno;
         if (verbose)
            fprintf(stderr, "Error reading file %s: %s\n", fn, strerror(-rc));
      }
      free(line);
      rc = linectr;

      fclose(fp);
   }
   if (debug)
      printf("(%s) Done. returning: %d\n", __func__, rc);
   return rc;
}


/* Reads the contents of a single line file.
 *
 * Arguments:
 *    fn          file name
 *    verbose     if true, write message to stderr if unable to open file
 *
 *  Returns:
 *    pointer to line read, caller responsible for freeing
 *    NULL if error or no lines in file
 */
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
      fclose(fp);
   }
   return single_line;
}


int rpt_file_contents(const char * fn, int depth) {
   GPtrArray * line_array = g_ptr_array_new();
   int rc = file_getlines(fn, line_array, false);
   if (rc < 0) {
      rpt_vstring(depth, "Error reading file %s: %s", fn, strerror(-rc));
   }
   else if (rc > 0) {
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


/* Scans list of directories to obtain for file names matching a criterion
 *
 * Arguments:
 *   dirnames     null terminated array of pointers to directory names
 *   filter_func  tests directory entry
 *
 * Returns:   GPtrArray of fully qualified file names
 *
 * Adapted from usbmonctl
 */
GPtrArray * get_filenames_by_filter(const char * dirnames[], Dirent_Filter filter_func) {
   // const char *hiddev_paths[] = { "/dev/", "/dev/usb/", NULL };
   bool debug = true;
   GPtrArray * devnames =  g_ptr_array_new();
   char path[PATH_MAX];

   for (int i = 0; dirnames[i] != NULL; i++) {
      struct dirent ** filelist;

      int count = scandir(dirnames[i], &filelist, filter_func, alphasort);
      if (count < 0) {
         assert(count == -1);
         fprintf(stderr, "(%s) scandir() error: %s\n", __func__, strerror(errno));
         continue;
      }
      for (int j = 0; j < count; j++) {
         snprintf(path, PATH_MAX, "%s%s", dirnames[i], filelist[j]->d_name);
         g_ptr_array_add(devnames, strdup(path));
         free(filelist[j]);
      }
      free(filelist);
   }

   if (debug) {
      printf("(%s) Found %d device names:\n", __func__, devnames->len);
      for (int ndx = 0; ndx < devnames->len; ndx++)
         printf("   %s\n", (char *) g_ptr_array_index(devnames, ndx) );
   }
   return devnames;
}

