/** \file file_util.c
 *  File utility functions
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/** \endcond */

#include "string_util.h"
#include "report_util.h"

#include "file_util.h"


/** Reads the lines of a text file into a GPtrArray.
 *
 *  @param  fn          file name
 *  @param  line_array  pointer to GPtrArray of strings where lines will be saved
 *  @param  verbose     if true, write message to stderr if unable to open file or other error
 *
 *  @retval >=0:  number of lines added to line_array
 *  @retval <0    -errno
 *
 *  The caller is responsible for freeing the lines added to line_array.
 */
int
file_getlines(
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
      if (verbose)
         fprintf(stderr, "Error opening file %s: %s\n", fn, strerror(errsv));
   }
   else {
      // if line == NULL && len == 0, then getline allocates buffer for line
      char * line = NULL;
      size_t len = 0;
      int     linectr = 0;
      errno = 0;
      while (getline(&line, &len, fp) >= 0) {
         linectr++;
         rtrim_in_place(line);     // strip trailing newline
         g_ptr_array_add(line_array, line);
         // printf("(%s) Retrieved line of length %zu: %s\n", __func__, read, line);
         line = NULL;  // reset for next getline() call
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


/** Reads the lines of a text file into a GPtrArray, returning a #Error_Info struct
 *  if an error occurs.
 *
 *  @param  fn          file name
 *  @param  lines       pointer to GPtrArray of strings where lines will be saved
 *
 *  @return NULL if success, newly allocated #Error_Info struct if error
 *
 *  The caller is responsible for freeing the lines added to line_array.
 *
 *  @todo
 *  Consider reimplementing with modified code of file_getlines(), to return more granular #Error_Info
 */
Error_Info *
file_getlines_errinfo(
      const char *  filename,
      GPtrArray *   lines)
{
   Error_Info * errs = NULL;

   int rc = file_getlines(filename,  lines, false);
   if (rc < 0) {
      char * detail = g_strdup_printf("Error reading file %s", filename);
      errs = errinfo_new2(
            rc,
            __func__,
            detail);
      free(detail);
   }
   return errs;
}


typedef struct {
   char **  lines;
   int      size;
   int      ct;
} Circular_String_Buffer;


/** Allocates a new #Circular_String_Buffer
 *
 *  @param  size  buffer size (number of entries)
 *  @return newly allocated #Circular_String_Buffer
 */
Circular_String_Buffer *
csb_new(int size) {
   Circular_String_Buffer * csb = calloc(1, sizeof(Circular_String_Buffer));
   csb->lines = calloc(size, sizeof(char*));
   csb->size = size;
   csb->ct = 0;
   return csb;
}


/** Appends a string to a #Circular_String_Buffer.
 *
 *  \param   csb   #Circular_String_Buffer
 *  \param   line  string to append
 *  \param   copy  if true, a copy of the string is appended to the buffer
 *                 if false, the string itself is appended
 */
void
csb_add(Circular_String_Buffer * csb, char * line, bool copy) {
    int nextpos = csb->ct % csb->size;
    // printf("(%s) Adding at ct %d, pos %d, line |%s|\n", __func__, csb->ct, nextpos, line);
    if (csb->lines[nextpos])
       free(csb->lines[nextpos]);
    if (copy)
       csb->lines[nextpos] = g_strdup(line);
    else
       csb->lines[nextpos] = line;
    csb->ct++;
}


/** All the strings in a #Circular_String_Buffer are moved to  a newly
 *  allocated GPtrArray. The count of lines in the now empty #Circular_String_Buffer
 *  is set to 0.
 *
 *   \param csb #Circular_String_Buffer to convert
 *   \return    newly allocated #GPtrArray
 */
GPtrArray *
csb_to_g_ptr_array(Circular_String_Buffer * csb) {
   // printf("(%s) csb->size=%d, csb->ct=%d\n", __func__, csb->size, csb->ct);
   GPtrArray * pa = g_ptr_array_sized_new(csb->ct);

   int first = 0;
   if (csb->ct > csb->size)
      first = csb->ct % csb->size;
   // printf("(%s) first=%d\n", __func__, first);

   for (int ndx = 0; ndx < csb->ct; ndx++) {
      int pos = (first + ndx) % csb->size;
      char * s = csb->lines[pos];
      // printf("(%s) line %d, |%s|\n", __func__, ndx, s);

      g_ptr_array_add(pa, s);
   }
   csb->ct = 0;
   return pa;
}


/** Reads the last lines of a text file into a GPtrArray.
 *
 *  @param  fn          file name
 *  @param  line_array  pointer to GPtrArray of strings where lines will be saved
 *  @param  verbose     if true, write message to stderr if unable to open file or other error
 *
 *  @retval >=0:  number of lines added to line_array
 *  @retval <0    -errno
 *
 *  The caller is responsible for freeing the lines added to line_array.
 */
int
file_get_last_lines(
      const char * fn,
      int          maxlines,
      GPtrArray**  line_array_loc,
      bool         verbose)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. fn=%s, maxlines=%d\n", __func__, fn, maxlines );

   int rc = 0;
   FILE * fp = fopen(fn, "r");
   if (!fp) {
      int errsv = errno;
      rc = -errno;
      if (verbose)
         fprintf(stderr, "Error opening file %s: %s\n", fn, strerror(errsv));
   }
   else {
      Circular_String_Buffer* csb = csb_new(maxlines);
      // if line == NULL && len == 0, then getline allocates buffer for line
      char * line = NULL;
      size_t len = 0;
      int    linectr = 0;
      errno = 0;
      // line == NULL and len == 0 => getline() allocates buffer, caller must free
      while (getline(&line, &len, fp) >= 0) {
         linectr++;
         rtrim_in_place(line);     // strip trailing newline
         csb_add(csb, line, /*copy=*/ true);

         // printf("(%s) Retrieved line of length %zu: %s\n", __func__, read, line);
         line = NULL;  // reset for next getline() call
         len  = 0;
      }
      if (errno != 0)  {   // getline error?
         rc = -errno;
         if (verbose)
            fprintf(stderr, "Error reading file %s: %s\n", fn, strerror(-rc));
      }
      free(line);
      rc = linectr;
      if (debug)
         printf("(%s) Read %d lines\n", __func__, linectr);
      if (rc > maxlines)
         rc = maxlines;

      *line_array_loc = csb_to_g_ptr_array(csb);
      free(csb);
//      if (debug) {
//         GPtrArray * la = *line_array_loc;
//         printf("(%s) (*line_array_loc)->len=%d\n", __func__, la->len);
//         if (la->len > 0)
//            printf("(%s) Line 0: %s\n", __func__, (char*)g_ptr_array_index(la, 0));
//         if (la->len > 1)
//            printf("(%s) Line 1: %s\n", __func__, (char*)g_ptr_array_index(la, 1));
//         if (la->len > 2) {
//            printf("(%s) Line %d: %s\n", __func__, la->len-2, (char*)g_ptr_array_index(la, la->len-2));
//            printf("(%s) Line %d: %s\n", __func__, la->len-1, (char*)g_ptr_array_index(la, la->len-1));
//         }
//      }

      fclose(fp);
   }

   if (debug)
      printf("(%s) Done. returning: %d\n", __func__, rc);
   return rc;
}


/** Reads the first line of a file.
 *
 *  @param  fn          file name
 *  @param  verbose     if true, write message to stderr if unable to open file
 *
 *  @retval non-NULL pointer to line read (caller responsible for freeing)
 *  @retval NULL     if error or no lines in file
 */
char *
file_get_first_line(
      const char * fn,
      bool         verbose)
{
   FILE * fp = fopen(fn, "r");
   char * single_line = NULL;
   if (!fp) {
      if (verbose)
         fprintf(stderr, "Error opening %s: %s\n", fn, strerror(errno));  // TODO: strerror() not thread safe
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
   // printf("(%s) fn=|%s|, returning: |%s|\n", __func__, fn, single_line);
   return single_line;
}


/** Reads a binary file, returning it as a **GByteArray**.
 *
 *  \param  fn        file name
 *  \param  est_size  estimated size
 *  \param  verbose   if open fails, write message to stderr
 *  \return if successful, a **GByteArray** of bytes, caller is responsible for freeing
 *          if failure, then NULL
 */
GByteArray *
read_binary_file(
      const char * fn,
      int          est_size,
      bool         verbose)
{
   assert(fn);

   bool debug = false;

   // DBGMSG("fn=%s", fn);

   Byte  buf[1];

   GByteArray * gbarray = NULL;

   FILE * fp;
   if (!(fp = fopen(fn, "r"))) {
      int errsv = errno;
      if (verbose){
         fprintf(stderr, "Error opening \"%s\", %s\n", fn, strerror(errsv));
      }
      goto bye;
   }

   if (est_size <= 0)
      gbarray = g_byte_array_new();
   else
      gbarray = g_byte_array_sized_new(est_size);

   // TODO: read bigger hunks
   size_t ct = 0;
   while ( (ct = fread(buf, /*size*/ 1, /*nmemb*/ 1, fp) ) > 0) {
      assert(ct == 1);
      g_byte_array_append(gbarray, buf, ct);
   }
   fclose(fp);

bye:
   if (debug) {
      if (gbarray)
         printf("(%s) Returning GByteArray of size %d", __func__, gbarray->len);
      else
         printf("(%s) Returning NULL", __func__);
      }
   return gbarray;
}


/** Checks if a regular file exists.
 *
 * @param fqfn fully qualified file name
 * @return     true/false
 */
bool
regular_file_exists(const char * fqfn) {
   bool result = false;
   struct stat stat_buf;
   int rc = stat(fqfn, &stat_buf);
   if (rc == 0) {
      result = S_ISREG(stat_buf.st_mode);
   }
   return result;
}


/** Checks if a directory exists.
 *
 * @param fqfn fully qualified directory name
 * @return     true/false
 */
bool
directory_exists(const char * fqfn) {
   bool result = false;
   struct stat stat_buf;
   int rc = stat(fqfn, &stat_buf);
   if (rc == 0) {
      result = S_ISDIR(stat_buf.st_mode);
   }
   return result;
}


/** Scans list of directories to obtain file names matching a criterion
 *
 *  @param dirnames     null terminated array of pointers to directory names
 *  @param filter_func  tests directory entry
 *
 *  @return  GPtrArray of fully qualified file names
 *
 *  A free function is set on the returned GPtrArray, so g_ptr_array_free() releases
 *  all the file names
 *
 * Adapted from usbmonctl
 */
GPtrArray *
get_filenames_by_filter(
      const char *  dirnames[],
      Dirent_Filter filter_func)
{
   // const char *hiddev_paths[] = { "/dev/", "/dev/usb/", NULL };
   bool debug = false;
   GPtrArray * devnames =  g_ptr_array_new();
   g_ptr_array_set_free_func(devnames, free);
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
      printf("(%s) Found %d file names:\n", __func__, devnames->len);
      for (int ndx = 0; ndx < devnames->len; ndx++)
         printf("   %s\n", (char *) g_ptr_array_index(devnames, ndx) );
   }
   return devnames;
}


/** Gets the file name for a file descriptor
 *
 * @param  fd    file descriptor
 * @param  p_fn  where to return a pointer to the file name
 *               The caller is responsible for freeing this memory
 *
 * @retval 0      success
 * @retval -errno if error (see readlink() doc for possible error numbers)
 */
int
filename_for_fd(int fd, char** p_fn) {
   char * result = calloc(1, PATH_MAX+1);
   char workbuf[40];
   int rc = 0;
   snprintf(workbuf, 40, "/proc/self/fd/%d", fd);
   ssize_t ct = readlink(workbuf, result, PATH_MAX);
   if (ct < 0) {
      rc = -errno;
      free(result);
      *p_fn = NULL;
   }
   else {
      assert(ct <= PATH_MAX);
      result[ct] = '\0';
      *p_fn = result;
   }
   // printf("(%s) fd=%d, returning: %d, *pfn=%p -> |%s|\n",
   //        __func__, fd, rc, *pfn, *pfn);
   return rc;
}


/** Gets the file name for a file descriptor.
 *
 *  The value returned is valid until the next call to this function
 *  in the current thread.
 *
 * @param  fd    file descriptor
 * @return       file name, NULL if error
 */
char *
filename_for_fd_t(int fd) {
   static GPrivate  key = G_PRIVATE_INIT(g_free);
   char * fn_buf = get_thread_fixed_buffer(&key, PATH_MAX+1);

   char * result = NULL;  // value to return

   char * filename_loc;
   int rc = filename_for_fd(fd, &filename_loc);
   if (rc == 0) {
      g_strlcpy(fn_buf, filename_loc, PATH_MAX+1);
      free(filename_loc);
      result = fn_buf;
   }
   return result;
}


/** Handles the boilerplate of iterating over a directory.
 *
 *  \param   dirname     directory name
 *  \param   fn_filter   tests the name of a file in a directory to see if should
 *                       be processed.  If NULL, all files are processed.
 *  \param   func        function to be called for each filename in the directory
 *  \param   accumulator pointer to a data structure passed
 *  \param   depth       logical indentation depth
 */
void
dir_foreach(
      const char *         dirname,
      Filename_Filter_Func fn_filter,
      Dir_Foreach_Func     func,
      void *               accumulator,
      int                  depth)
{
   struct dirent *dent;
   DIR           *d;
   d = opendir(dirname);
   if (!d) {
      rpt_vstring(depth,"Unable to open directory %s: %s", dirname, strerror(errno));
   }
   else {
      while ((dent = readdir(d)) != NULL) {
         // DBGMSG("%s", dent->d_name);
         if (!streq(dent->d_name, ".") && !streq(dent->d_name, "..") ) {
            if (!fn_filter || fn_filter(dent->d_name)) {
               func(dirname, dent->d_name, accumulator, depth);
            }
         }
      }
      closedir(d);
   }
}


/** Iterates over a directory in an ordered manner.
 *
 *  \param   dirname      directory name
 *  \param   fn_filter    tests the name of a file in a directory to see if should
 *                        be processed.  If NULL, all files are processed.
 *  \param   compare_func qsort style function to compare filenames. If NULL perform string comparison
 *  \param   func         function to be called for each filename in the directory
 *  \param   accumulator  pointer to a data structure passed
 *  \param   depth        logical indentation depth
 */
void
dir_ordered_foreach(
        const char *          dirname,
        Filename_Filter_Func  fn_filter,
        GCompareFunc          compare_func,
        Dir_Foreach_Func      func,
        void *                accumulator,
        int                   depth)
{
   GPtrArray * simple_filenames = g_ptr_array_new();

   struct dirent *dent;
   DIR           *d;
   d = opendir(dirname);
   if (!d) {
      rpt_vstring(depth,"Unable to open directory %s: %s", dirname, strerror(errno));
   }
   else {
      while ((dent = readdir(d)) != NULL) {
         // DBGMSG("%s", dent->d_name);
         if (!streq(dent->d_name, ".") && !streq(dent->d_name, "..") ) {
            if (!fn_filter || fn_filter(dent->d_name)) {
               g_ptr_array_add(simple_filenames, strdup(dent->d_name));
            }
         }
      }
      closedir(d);

      if (compare_func)
         g_ptr_array_sort(simple_filenames, compare_func);
      else
         g_ptr_array_sort(simple_filenames, indirect_strcmp);

      for (int ndx = 0; ndx < simple_filenames->len; ndx++) {
         char * fn = g_ptr_array_index(simple_filenames, ndx);
         func(dirname, fn, accumulator, depth);
      }
   }
}

