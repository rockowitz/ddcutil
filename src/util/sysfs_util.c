/* sysfs_util.c
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

/** \file
 * Functions for reading /sys file system
 */

#include <assert.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_util.h"

#include "sysfs_util.h"


/** Reads a /sys attribute file, which is 1 line of text
 *
 * \param  dirname    directory name
 * \param  attrname   attribute name, i.e. file name
 * \param  verbose    if true, write message to stderr if unable to open file
 * \return pointer to attribute value string, caller is responsible for freeing
 */
char *
read_sysfs_attr(
      const char * dirname,
      const char * attrname,
      bool         verbose)
{
   char fn[PATH_MAX];
   sprintf(fn, "%s/%s", dirname, attrname);
   return file_get_first_line(fn, verbose);
}


/** Reads a /sys attribute file, which is 1 line of text.
 *  If the attribute is not found, returns a default value
 *
 * \param  dirname    directory name
 * \param  attrname   attribute name, i.e. file name
 * \param  default    default value, duplicated
 * \param  verbose    if true, write message to stderr if unable to open file
 * \return pointer to attribute value string, caller is responsible for freeing
 */
char *
read_sysfs_attr_w_default(
      const char * dirname,
      const char * attrname,
      const char * default_value,
      bool         verbose)
{
   char fn[PATH_MAX];
   sprintf(fn, "%s/%s", dirname, attrname);
   char * result = file_get_first_line(fn, verbose);
   if (!result)
      result = strdup(default_value);  // strdup() so caller can free any result
   return result;
}

char *
read_sysfs_attr_w_default_r(
      const char * dirname,
      const char * attrname,
      const char * default_value,
      char *       buf,
      unsigned     bufsz,
      bool         verbose)
{
   char fn[PATH_MAX];
   sprintf(fn, "%s/%s", dirname, attrname);
   char * result = file_get_first_line(fn, verbose);
   if (result) {
      g_strlcpy(buf, result, bufsz);
      free(result);
   }
   else {
      g_strlcpy(buf, default_value, bufsz);
   }
   return buf;
}



/** Reads a binary /sys attribute file
 *
 * \param  dirname    directory name
 * \param  attrname   attribute name, i.e. file name
 * \param  est_size  estimated size
 * \param  verbose   if open fails, write message to stderr
 * \return if successful, a #GByteArray of bytes, caller is responsible for freeing
 *          if failure, then NULL
 */
GByteArray *
read_binary_sysfs_attr(
      const char * dirname,
      const char * attrname,
      int          est_size,
      bool         verbose)
{
   assert(dirname);
   assert(attrname);

   char fn[PATH_MAX];
   sprintf(fn, "%s/%s", dirname, attrname);

   return read_binary_file(fn, est_size, verbose);
}


/** Looks in the /sys file system to check if a module is loaded.
 *
 * \param  module_name    module name
 * \return true if the module is loaded, false if not
 */
bool
is_module_loaded_using_sysfs(
      const char * module_name)
{
   bool debug = false;

   struct stat statbuf;
   char   module_fn[100];
   bool   found = false;

   snprintf(module_fn, sizeof(module_fn), "/sys/module/%s", module_name);
   int rc = stat(module_fn, &statbuf);
   if (rc < 0) {
      // will be ENOENT (2) if file not found
      found = false;
   }
   else {
      // if (S_ISDIR(statbuf.st_mode))   // pointless
         found = true;
   }

   if (debug)
      printf("(%s) module_name = %s, returning %d", __func__, module_name, found);
   return found;
}

