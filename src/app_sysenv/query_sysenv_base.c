/* query_sysenv_base.c
 *
 * Created on: Nov 9, 2017
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

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "util/data_structures.h"
#include "util/file_util.h"
#include "util/report_util.h"
#include "util/string_util.h"

#include "query_sysenv_base.h"



static char * known_video_driver_modules[] = {
      "amdgpu",
      "fbdev",
      "fglrx",
      "fturbo",
      "i915",
      "mgag200",
      "nvidia",
      "nouveau",
      "radeon",
      "vboxvideo",
      "vc4",
      NULL
};

static char * prefix_matches[] = {
      "amdgpu",
      "drm",
      "i2c",
      "video",
      "vc4",
      NULL
};

static char * other_driver_modules[] = {
      "drm",
  //  "eeprom",       // not really interesting
      "i2c_algo_bit",
      "i2c_dev",
      "i2c_piix4",
      NULL
};

char ** get_known_video_driver_modules() {
   return known_video_driver_modules;
}

char ** get_prefix_matches() {
   return prefix_matches;
}

char ** get_other_driver_modules() {
   return other_driver_modules;
}







void report_file_first_line(char * fn, char * title, int depth) {
   int d1 = depth+1;
   if (title)
      rpt_title(title, depth);
   else
      rpt_vstring(depth, "%s:", fn);

   char * s = file_get_first_line(fn, true /* verbose */);
   if (s)
      rpt_title(s, d1);
   else
      rpt_vstring(d1, "Unable to read %s", fn);
}


bool show_one_file(char * dir_name, char * simple_fn, bool verbose, int depth) {
   bool result = false;
   char fqfn[PATH_MAX+2];
   strcpy(fqfn,dir_name);
   if (!str_ends_with(dir_name, "/"))
      strcat(fqfn,"/");
   assert(strlen(fqfn) + strlen(simple_fn) <= PATH_MAX);   // for Coverity
   strncat(fqfn,simple_fn, sizeof(fqfn)-(strlen(fqfn)+1));  // use strncat to make Coverity happy
   if (regular_file_exists(fqfn)) {
      rpt_vstring(depth, "%s:", fqfn);
      rpt_file_contents(fqfn, /*verbose=*/true, depth+1);
      result = true;
   }
   else if (verbose)
      rpt_vstring(depth, "File not found: %s", fqfn);
   return result;
}





void free_env_accumulator(Env_Accumulator * accum) {
   if (accum) {
      free(accum->architecture);
      free(accum->distributor_id);
      if (accum->i2c_device_numbers)
         bva_free(accum->i2c_device_numbers);
      if (accum->driver_list)
         free_driver_name_list(accum->driver_list);
      free(accum);
   }
}



// Functions to query and free the linked list of driver names.
// The list is created by executing function query_card_and_driver_using_sysfs(),
// which is grouped with the sysfs functions.

/** Frees the driver name list created by query_card_and_driver_using_sysfs()
 *
 * \param driver_list     pointer to head of linked list of driver names
 */
void free_driver_name_list(struct driver_name_node * driver_list) {
   // Free the driver list
   struct driver_name_node * cur_node = driver_list;
   while (cur_node) {
      struct driver_name_node * next_node = cur_node->next;
      free(cur_node);
      cur_node = next_node;
   }
}




/** Handles the boilerplate of iterating over a directory.
 *
 *  \param   dirname     directory name
 *  \param   fn_filter   tests the name of a file in a directory to see if should
 *                       be processe.  If NULL, all files are processed.
 *  \param   func        function to be called for each filename in the directory
 *  \param   accumulator pointer to a data structure passed
 *  \param   depth       logical indentation depth
 */
void dir_foreach(
      char * dirname,
      Filename_Filter_Func fn_filter,
      Dir_Foreach_Func func,
      void * accumulator,
      int depth)
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
   }
}


