/* file_util.h
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** @file file_util.h
 * File utility functions
 */

#ifndef FILE_UTIL_H_
#define FILE_UTIL_H_

/** \cond */
#include <dirent.h>
#include <glib-2.0/glib.h>
#include <stdbool.h>
/** \endcond */

#include "error_info.h"

int file_getlines(const char * fn, GPtrArray* line_array, bool verbose);
Error_Info * file_getlines_errinfo(const char *  filename, GPtrArray *   lines);
char * file_get_first_line(const char * fn, bool verbose);

int
file_get_last_lines(
      const char * fn,
      int          maxlines,
      GPtrArray**  line_array_loc,
      bool         verbose);

GByteArray * read_binary_file(char * fn, int est_size, bool verbose);

bool regular_file_exists(const char * fqfn);
bool directory_exists(const char * fqfn);

/** Filter function for get_filenames_by_filter() */
typedef int (*Dirent_Filter)(const struct dirent *end);
GPtrArray * get_filenames_by_filter(const char * dirnames[], Dirent_Filter filter_func);

int filename_for_fd(int fd, char** p_fn);
char * filename_for_fd_t(int fd);


#endif /* FILE_UTIL_H_ */
