/** \file file_util.h
 *  File utility functions
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FILE_UTIL_H_
#define FILE_UTIL_H_

/** \cond */
#include <dirent.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
#include <stdio.h>
/** \endcond */

#include "error_info.h"

int file_getlines(
      const char *  fn,
      GPtrArray*    line_array,
      bool          verbose);

Error_Info * file_getlines_errinfo(
      const char *  filename,
      GPtrArray *   lines);

char * file_get_first_line(
      const char *  fn,
      bool          verbose);

int file_get_last_lines(
      const char *  fn,
      int           maxlines,
      GPtrArray**   line_array_loc,
      bool          verbose);

GByteArray * read_binary_file(
      const char *  fn,
      int           est_size,
      bool          verbose);

bool regular_file_exists(
      const char *  fqfn);

bool directory_exists(
      const char *  fqfn);

/** Signature of Filter function for get_filenames_by_filter() */
typedef int (*Dirent_Filter)(
      const struct dirent *end);

GPtrArray * get_filenames_by_filter(
      const char *  dirnames[],
      Dirent_Filter filter_func);

int filename_for_fd(
      int           fd,
      char**        fn_loc);

char * filename_for_fd_t(
      int           fd);

/** Signature of filename filter function passed to #dir_foreach(). */
typedef bool (*Filename_Filter_Func)(
      const char *  simple_fn);

/** Signature of function called by #dir_foreach to process each file. */
typedef void (*Dir_Foreach_Func)(
      const char *  dirname,
      const char *  fn,
      void *        accumulator,
      int           depth);

void dir_foreach(
      const char *          dirname,
      Filename_Filter_Func  fn_filter,
      Dir_Foreach_Func      func,
      void *                accumulator,
      int                   depth);

void dir_ordered_foreach(
      const char *          dirname,
      Filename_Filter_Func  fn_filter,
      GCompareFunc          compare_func,
      Dir_Foreach_Func      func,
      void *                accumulator,
      int                   depth);

void filter_and_limit_g_ptr_array(
      GPtrArray * line_array,
      char **     filter_terms,
      bool        ignore_case,
      int         limit);

int read_file_with_filter(
      GPtrArray *  line_array,
      const char * fn,
      char **      filter_terms,
      bool         ignore_case,
      int          limit);

int rek_mkdir(
      const char * path,
      FILE *       ferr);

int fopen_mkdir(
      const char * path,
      const char * mode,
      FILE *       ferr,
      FILE **      fp_loc);

#endif /* FILE_UTIL_H_ */
