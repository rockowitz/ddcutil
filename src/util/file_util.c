/** \file file_util.c
 *  File utility functions
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "data_structures.h"
#include "report_util.h"
#include "string_util.h"

#include "file_util.h"


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
         // printf("(%s) Retrieved line of length %zu, trimmed length %zu: %s\n",
         //           __func__, len, strlen(line), line);
         line = NULL;  // reset for next getline() call
         len  = 0;
      }
      // assert(getline_rc < 0);
      if (errno != 0) {   // was it an error or eof?
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
      STRLCPY(fn_buf, filename_loc, PATH_MAX+1);
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


/** Reads the contents of a file into a #GPtrArray of lines, optionally keeping only
 *  those lines containing at least one in a list of terms.  After filtering, the set
 *  of returned lines may be further reduced to either the first or last N number of
 *  lines.
 *
 *  \param  line_array    #GPtrArray in which to return the lines read
 *  \param  fn            file name
 *  \param  filter_terms  #Null_Terminated_String_Away of filter terms
 *  \param  ignore_case   ignore case when testing filter terms
 *  \param  limit if 0, return all lines that pass filter terms
 *                if > 0, return at most the first #limit lines that satisfy the filter terms
 *                if < 0, return at most the last  #limit lines that satisfy the filter terms
 *  \return if >= 0, number of lines before filtering and limit applied
 *          if < 0,  -errno, from file_getlines(), i.e. fopen(), getline()
 *
 *   **line_array** is emptied at start of function execution.
 *  Lines read are appended to existing lines in line_array
 *  \remark
 *  This function was created because using grep in conjunction with pipes was
 *  producing obscure shell errors.
 *  \remark
 *  Returning the count of unfiltered lines is a bit odd, but the caller can
 *  get the filtered count from line_array->len
 */
int read_file_with_filter(
      GPtrArray *  line_array,
      const char * fn,
      char **      filter_terms,
      bool         ignore_case,
      int          limit)
{
   bool debug = false;
   if (debug) {
      printf("(%s) line_array=%p, fn=%s, ct(filter_terms)=%d, ignore_case=%s, limit=%d\n",
             __func__, line_array, fn, ntsa_length(filter_terms), sbool(ignore_case), limit);
      if (ntsa_length(filter_terms) > 0) {
         printf("(%s) filter_terms:\n", __func__);
         for (char ** term_ptr = filter_terms; *term_ptr; term_ptr++)
            printf("(%s)    |%s|\n", __func__, *term_ptr);
      }
   }

   g_ptr_array_set_free_func(line_array, g_free);    // in case not already set
   g_ptr_array_remove_range(line_array, 0, line_array->len);

   int rc = file_getlines(fn, line_array, /*verbose*/ false);
   if (debug)
      printf("(%s) file_getlines() returned %d\n", __func__, rc);

   if (rc > 0) {
      filter_and_limit_g_ptr_array(
         line_array,
         filter_terms,
         ignore_case,
         limit);
   }
   else { // rc == 0
      if (debug)
         printf("(%s) Empty file\n", __func__);
   }

   if (debug)
      printf("(%s) Done. Returning: %d\n", __func__, rc);
   return rc;
}


/** Deletes lines from a #GPtrArray of text lines. If filter terms
 *  are specified, lines not satisfying any of the search terms are
 *  deleted.  Then, if **limit** is specified, at most the limit
 *  number of lines are left.
 *
 *  \param line_array   GPtrArray of null-terminated strings
 *  \param filter_terms null-terminated string array of terms
 *  \param ignore_case  if true, ignore case when testing filter terms
 *  \param limit  if 0,   return all lines that pass filter terms
 *                if > 0, return at most the first #limit lines that satisfy the filter terms
 *                if < 0, return at most the last  #limit lines that satisfy the filter terms
 *
 *  \remark
 *  Consider allowing filter_terms to be regular expressions.
 */
void filter_and_limit_g_ptr_array(
      GPtrArray * line_array,
      char **     filter_terms,
      bool        ignore_case,
      int         limit)
{
//   bool debug = false;
//   if (debug) {
//      DBGMSG("line_array=%p, line_array->len=%d, ct(filter_terms)=%d, ignore_case=%s, limit=%d",
//            line_array, line_array->len, ntsa_length(filter_terms), sbool(ignore_case), limit);
//      // (const char **) cast to conform to strjoin() signature
//      char * s = strjoin( (const char **) filter_terms, -1, ", ");
//      DBGMSG("Filter terms: %s", s);
//      free(s);
//   };
#ifdef TOO_MUCH
   if (debug) {
      if (filter_terms) {
         printf("(%s) filter_terms:\n", __func__);
         ntsa_show(filter_terms);
      }
   }
#endif
   // inefficient, just make it work for now
   for (int ndx = (line_array->len)-1 ; ndx >= 0; ndx--) {
      char * s = g_ptr_array_index(line_array, ndx);
      assert(s);
      // DBGMSF(debug, "s=|%s|", s);
      bool keep = true;
      if (filter_terms)
         keep = apply_filter_terms(s, filter_terms, ignore_case);
      if (!keep) {
         g_ptr_array_remove_index(line_array, ndx);
      }
   }
   gaux_ptr_array_truncate(line_array, limit);

   // DBGMSF(debug, "Done. line_array->len=%d", line_array->len);
}


/** Given a directory, if the directory does not already exist,
 *  creates the directory along with any required parent directories.
 *
 *  \param  path
 *  \ferr   if non-null, destination for error messages
 *  \return 0 if success, -errno if error
 *
 *  \remark
 *  Based on answer by Jens Harms to
 *  https://stackoverflow.com/questions/7430248/creating-a-new-directory-in-c
 */
int rek_mkdir(
      const char *path,
      FILE *      ferr)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting, path=%s\n", __func__, path);

   int result = 0;
   if (!directory_exists(path)) {
      char *sep = strrchr(path, '/');
      if (sep) {
         *sep = 0;
         result = rek_mkdir(path, ferr);  // create parent dir
         *sep = '/';
      }
      if (result == 0) {
         if (debug)
            printf("(%s) Creating path %s\n", __func__, path);
         if ( mkdir(path, 0777) < 0) {
            result = -errno;
            f0printf(ferr, "Unable to create '%s', %s\n", path, strerror(errno));
         }
      }
   }

   if (debug)
      printf("(%s) Done. returning %d\n", __func__, result);
   return result;
}


/** Opens a file for writing, creating parent directories if necessary.
 *
 *  \param  path
 *  \param  mode
 *  \param  ferr   if non-null, destination for error messages
 *  \param  fp_loc address at which a pointer to the open file is returned
 *  \return 0 if successful, -errno if error
 */
int fopen_mkdir(
      const char *path,
      const char *mode,
      FILE       *ferr,
      FILE      **fp_loc)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. path=%s, mode=%s, fp_loc=%p\n", __func__, path, mode, fp_loc);

   int rc = 0;
   *fp_loc = NULL;
   char *sep = strrchr(path, '/');
   if (sep) {
      char *path0 = strdup(path);
      path0[ sep - path ] = 0;
      rc = rek_mkdir(path0, ferr);
      free(path0);
   }
   if (!rc) {
      *fp_loc = fopen(path,mode);
      if (!*fp_loc) {
         rc = -errno;
         f0printf(ferr, "Unable to open %s with mode %s: %s\n", path, mode, strerror(errno));
      }
   }
   assert( (rc == 0 && *fp_loc) || (rc != 0 && !*fp_loc ) );

   if (debug)
       printf("(%s) Done. returning %d\n", __func__, rc);
   return rc;
}

