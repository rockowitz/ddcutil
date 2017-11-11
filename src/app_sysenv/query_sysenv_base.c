/* query_sysenv_base.c
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

/** \f
 *  Common sysenv functions
 */
/** \cond */
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
/** \endcond */

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

/** Returns the null terminated list of known video driver names */
char ** get_known_video_driver_module_names() {
   return known_video_driver_modules;
}

/** Returns the null terminated list of match prefixes */
char ** get_prefix_match_names() {
   return prefix_matches;
}

/** Returns the null terminated list of names of other drivers of interest */
char ** get_other_driver_module_names() {
   return other_driver_modules;
}


/** Reports the first line of a file, indented under a title.
 *  Issues a message if unable to read the file.
 *
 *  \param fn    file name
 *  \param title title message
 *  \param depth logical indentation depth
 */
void sysenv_rpt_file_first_line(char * fn, char * title, int depth) {
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


/** Reports the contents of a file.
 *
 *  \param dir_name  directory name
 *  \param simple_fn simple file name
 *  \param verbose  if ***true***, issue message if error
 *  \param depth    logical indentation depth
 */
bool sysenv_show_one_file(char * dir_name, char * simple_fn, bool verbose, int depth) {
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


/** Allocates and initializes a #Env_Accumulator data structure
 *
 *  \return newly allocated struct
 */
Env_Accumulator * env_accumulator_new() {
   Env_Accumulator * accum = calloc(1, sizeof(Env_Accumulator));
   memcpy(accum->marker, ENV_ACCUMULATOR_MARKER, 4);
   return accum;
}


/** Frees the #Env_Accumulator data structure.
 *
 *  \param accum pointer to data structure
 */
void env_accumulator_free(Env_Accumulator * accum) {
   if (accum) {
      free(accum->architecture);
      free(accum->distributor_id);
      if (accum->dev_i2c_device_numbers)
         bva_free(accum->dev_i2c_device_numbers);
      if (accum->driver_list)
         driver_name_list_free(accum->driver_list);
      if (accum->sys_bus_i2c_device_numbers)
         bva_free(accum->sys_bus_i2c_device_numbers);
      free(accum);
   }
}

/*** Debugging report for the **Env_Accumulator** struct
 *
 *   @param accum  pointer to data structure
 *   @param depth  logical indentation depth
 */
void env_accumulator_report(Env_Accumulator * accum, int depth) {
   int d1 = depth+1;
   rpt_title("Env_Accumulator:", depth);
   rpt_vstring(d1, "%-30s %s", "architecture:", (accum->architecture) ? accum->architecture : "");
   rpt_vstring(d1, "%-30s %s", "distributor_id", (accum->distributor_id) ? accum->distributor_id : "");

   const int bufsz = 200;
   char buf[bufsz];
   buf[0] = '\0';
   if (accum->dev_i2c_device_numbers) {
      int len = bva_length(accum->dev_i2c_device_numbers);
      Byte * bytes = bva_bytes(accum->dev_i2c_device_numbers);
      for (int ndx = 0; ndx < len; ndx++) {
         snprintf(buf + strlen(buf), bufsz-strlen(buf), "%s%d",
                  (ndx == 0) ? "" : " ",
                  bytes[ndx]);
      }
   }
   assert(strlen(buf) < bufsz);
   rpt_vstring(d1, "%-30s %s", "/dev/i2c device numbers:", buf);

   char * driver_names = NULL;
   if (accum->driver_list)
      driver_names = driver_name_list_string(accum->driver_list);
   rpt_vstring(d1, "%-30s %s", "Drivers detected:", (driver_names) ? driver_names : "");
   if (driver_names)
      free(driver_names);
   rpt_vstring(d1, "%-30s %s", "sysfs_i2c_devices_exist:", bool_repr(accum->sysfs_i2c_devices_exist));

   buf[0] = '\0';
   if (accum->sys_bus_i2c_device_numbers) {
   int len = bva_length(accum->sys_bus_i2c_device_numbers);
      Byte * bytes = bva_bytes(accum->sys_bus_i2c_device_numbers);
      for (int ndx = 0; ndx < len; ndx++) {
         snprintf(buf + strlen(buf), bufsz-strlen(buf),
                  "%s%d",
                  (ndx == 0) ? "" : " ",
                  bytes[ndx]);
      }
   }
   assert(strlen(buf) < bufsz);
   rpt_vstring(d1, "%-30s %s", "/sys/bus/i2c device numbers:", buf);

#ifdef REF
   bool               group_i2c_exists;
   bool               cur_user_in_group_i2c;
   bool               cur_user_any_devi2c_rw;
   bool               cur_user_all_devi2c_rw;
#endif

   rpt_vstring(d1, "%-30s %s", "dev_i2c_devices_required:",       bool_repr(accum->dev_i2c_devices_required));
   rpt_vstring(d1, "%-30s %s", "group_i2c_exists:",       bool_repr(accum->group_i2c_exists));
   rpt_vstring(d1, "%-30s %s", "dev_i2c_common_group_name:",     accum->dev_i2c_common_group_name);
   rpt_vstring(d1, "%-30s %s", "cur_user_in_group_i2c:",  bool_repr(accum->cur_user_in_group_i2c));
   rpt_vstring(d1, "%-30s %s", "cur_user_any_devi2c_rw:", bool_repr(accum->cur_user_any_devi2c_rw));
   rpt_vstring(d1, "%-30s %s", "cur_user_all_devi2c_rw:", bool_repr(accum->cur_user_all_devi2c_rw));
   rpt_vstring(d1, "%-30s %s", "module_i2c_dev_needed:",  bool_repr(accum->module_i2c_dev_needed));
   rpt_vstring(d1, "%-30s %s", "module_i2c_dev_loaded:",  bool_repr(accum->module_i2c_dev_loaded));
   rpt_vstring(d1, "%-30s %s", "all_dev_i2c_has_group_i2c:",  bool_repr(accum->all_dev_i2c_has_group_i2c));
   rpt_vstring(d1, "%-30s %s", "any_dev_i2c_has_group_i2c:",  bool_repr(accum->any_dev_i2c_has_group_i2c));
   rpt_vstring(d1, "%-30s %s", "all_dev_i2c_is_group_rw:",  bool_repr(accum->all_dev_i2c_is_group_rw));
   rpt_vstring(d1, "%-30s %s", "any_dev_i2c_is_group_rw:",  bool_repr(accum->any_dev_i2c_is_group_rw));
}


// Functions to query and free the linked list of detected driver names.
// The list is created by executing function query_card_and_driver_using_sysfs(),
// which is grouped with the sysfs functions.

/** Searches the driver name list for a specified name
 *
 *  \param head        list head
 *  \param driver_name name of driver to search for
 *  \return pointer to node containing driver, NULL if not found
 */
Driver_Name_Node * driver_name_list_find(Driver_Name_Node * head, char * driver_name) {
   Driver_Name_Node * cur_node = head;
   while (cur_node && !streq(cur_node->driver_name, driver_name))
        cur_node = cur_node->next;
   return cur_node;
}


/** Adds a driver name to the head of the linked list of driver names.
 *
 *  If the specified name is already in the list it is not added again.
 *
 *  \param headptr pointer to address of head of the list
 *  \param driver_name name to add
 */
void driver_name_list_add(Driver_Name_Node ** headptr, char * driver_name) {
   // printf("(%s) Adding driver |%s|\n", __func__, driver_name);
   if (!driver_name_list_find(*headptr, driver_name)) {
      Driver_Name_Node * newnode = calloc(1, sizeof(Driver_Name_Node));
      newnode->driver_name = strdup(driver_name);
      newnode->next = *headptr;
      *headptr = newnode;
   }
}


/** Checks the list of detected drivers to see if AMD's proprietary
 * driver fglrx is the only driver.
 *
 * \param  driver_list     linked list of driver names
 * \return true if fglrx is the only driver, false otherwise
 */
bool only_fglrx(struct driver_name_node * driver_list) {
   int driverct = 0;
   bool fglrx_seen = false;
   struct driver_name_node * curnode = driver_list;
   while (curnode) {
      driverct++;
      if (str_starts_with(curnode->driver_name, "fglrx"))
         fglrx_seen = true;
      curnode = curnode->next;
   }
   bool result = (driverct == 1 && fglrx_seen);
   // DBGMSG("driverct = %d, returning %d", driverct, result);
   return result;
}


/** Checks the list of detected drivers to see if the proprietary
 *  AMD and Nvidia drivers are the only ones.
 *
 * \param  driver list        linked list of driver names
 * \return true  if both nvidia and fglrx are present and there are no other drivers,
 *         false otherwise
 */
bool only_nvidia_or_fglrx(struct driver_name_node * driver_list) {
   int driverct = 0;
   bool other_driver_seen = false;
   struct driver_name_node * curnode = driver_list;
   while (curnode) {
      driverct++;
      if (!str_starts_with(curnode->driver_name, "fglrx") &&
          !streq(curnode->driver_name, "nvidia")
         )
      {
         other_driver_seen = true;
      }
      curnode = curnode->next;
   }
   bool result = (!other_driver_seen && driverct > 0);
   // DBGMSG("driverct = %d, returning %d", driverct, result);
   return result;
}


// TODO: combine with driver_name_list_find(), add a boolean by_prefix or exact parm
/** Checks if any driver name in the list of detected drivers starts with
 * the specified string.
 *
 *  \param  driver list     linked list of driver names
 *  \parar  driver_prefix   driver name prefix
 *  \return true if found, false if not
 */
bool found_driver(struct driver_name_node * driver_list, char * driver_prefix) {
   bool found = false;
   struct driver_name_node * curnode = driver_list;
   while (curnode) {
      if ( str_starts_with(curnode->driver_name, driver_prefix) ) {
         found = true;
         break;
      }
      curnode = curnode->next;
   }
   // DBGMSG("driver_name=%s, returning %d", driver_prefix, found);
   return found;
}



/** Frees the driver name list
 *
 * \param driver_list pointer to head of linked list of driver names
 */
void driver_name_list_free(struct driver_name_node * driver_list) {
   // Free the driver list
   struct driver_name_node * cur_node = driver_list;
   while (cur_node) {
      free(cur_node->driver_name);
      struct driver_name_node * next_node = cur_node->next;
      free(cur_node);
      cur_node = next_node;
   }
}


/** Returns a comma delimited list of all the driver names in a
 *  driver name list.
 *
 *  \param head pointer to head of list
 */
char * driver_name_list_string(Driver_Name_Node * head) {
   int reqd_sz = 1;   // for trailing \0
   Driver_Name_Node * cur = head;
   while (cur) {
      // printf("(%s) driver_name: |%s|\n", __func__, cur->driver_name);
      reqd_sz += strlen(cur->driver_name);
      if (cur != head)
         reqd_sz += 2;   // for ", "
      cur = cur->next;
   }
   // printf("(%s) reqd_sz = %d\n", __func__, reqd_sz);
   char * result = malloc(reqd_sz);
   result[0] = '\0';
   cur = head;
   while(cur) {
      if (cur != head)
         strcat(result, ", ");
      strcat(result, cur->driver_name);
      cur = cur->next;
   }
   assert(strlen(result) == reqd_sz-1);
   // printf("(%s) result: |%s|\n", __func__, result);
   return result;
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
      char *               dirname,
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
   }
}
