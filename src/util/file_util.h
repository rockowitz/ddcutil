/* file_util.h
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

#ifndef FILE_UTIL_H_
#define FILE_UTIL_H_

#include <glib.h>
#include <stdbool.h>


int file_getlines(const char * fn, GPtrArray* line_array, bool verbose);

char * read_one_line_file(char * fn, bool verbose);

bool regular_file_exists(const char * fqfn);
bool directory_exists(const char * fqfn);

int rpt_file_contents(const char * fn, int depth);

#endif /* FILE_UTIL_H_ */
