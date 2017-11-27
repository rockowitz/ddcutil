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
#include "util/subprocess_util.h"

#include "base/core.h"
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
   // strcpy(fqfn,dir_name);
   g_strlcpy(fqfn, dir_name, PATH_MAX);  // make coverity happy
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

   // Defaults that differ from 0 values set by calloc():
   accum->dev_i2c_devices_required  = true;

   // will be set false if any instance fails the test
   accum->cur_user_all_devi2c_rw    = true;
   accum->all_dev_i2c_has_group_i2c = true;
   accum->all_dev_i2c_is_group_rw   = true;

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
      free(accum->cur_uname);
      free(accum->dev_i2c_common_group_name);
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

   char * dev_i2c_device_numbers_string = "";
   if (accum->dev_i2c_device_numbers)
      dev_i2c_device_numbers_string = bva_as_string(accum->dev_i2c_device_numbers, /*as_hex*/ false, " ");

   char * driver_names = "";
   if (accum->driver_list)
      driver_names = driver_name_list_string(accum->driver_list);

   char * sys_bus_i2c_device_numbers_string = "";
   if (accum->sys_bus_i2c_device_numbers)
      sys_bus_i2c_device_numbers_string = bva_as_string(accum->sys_bus_i2c_device_numbers, /*as_hex*/ false, " ");

   rpt_label(depth, "Env_Accumulator:");
   rpt_vstring(d1, "%-30s %s", "architecture:",  (accum->architecture)   ? accum->architecture   : "");
   rpt_vstring(d1, "%-30s %s", "distributor_id", (accum->distributor_id) ? accum->distributor_id : "");
   rpt_vstring(d1, "%-30s %s", "Drivers detected:",          driver_names);
   rpt_vstring(d1, "%-30s %s", "/dev/i2c device numbers:",   dev_i2c_device_numbers_string);
   rpt_vstring(d1, "%-30s %s", "sysfs_i2c_devices_exist:",   bool_repr(accum->sysfs_i2c_devices_exist));
   rpt_vstring(d1, "%-30s %s", "/sys/bus/i2c device numbers:", sys_bus_i2c_device_numbers_string);
   rpt_vstring(d1, "%-30s %s", "dev_i2c_devices_required:",  bool_repr(accum->dev_i2c_devices_required));
   rpt_vstring(d1, "%-30s %s", "module_i2c_dev_needed:",     bool_repr(accum->module_i2c_dev_needed));
   rpt_vstring(d1, "%-30s %s", "module_i2c_dev_builtin:",    bool_repr(accum->module_i2c_dev_builtin));
   rpt_vstring(d1, "%-30s %s", "loadable_i2c_dev_exists:",   bool_repr(accum->loadable_i2c_dev_exists));
   rpt_vstring(d1, "%-30s %s", "i2c_dev_loaded_or_builtin:", bool_repr(accum->i2c_dev_loaded_or_builtin));
   rpt_vstring(d1, "%-30s %s", "group_i2c_checked:",         bool_repr(accum->group_i2c_checked));
   rpt_vstring(d1, "%-30s %s", "group_i2c_exists:",          bool_repr(accum->group_i2c_exists));
   rpt_vstring(d1, "%-30s %s", "dev_i2c_common_group_name:", accum->dev_i2c_common_group_name);
   rpt_vstring(d1, "%-30s %s", "all_dev_i2c_has_group_i2c:", bool_repr(accum->all_dev_i2c_has_group_i2c));
   rpt_vstring(d1, "%-30s %s", "any_dev_i2c_has_group_i2c:", bool_repr(accum->any_dev_i2c_has_group_i2c));
   rpt_vstring(d1, "%-30s %s", "all_dev_i2c_is_group_rw:",   bool_repr(accum->all_dev_i2c_is_group_rw));
   rpt_vstring(d1, "%-30s %s", "any_dev_i2c_is_group_rw:",   bool_repr(accum->any_dev_i2c_is_group_rw));
   rpt_vstring(d1, "%-30s %s", "cur_uname:",                 accum->cur_uname);
   rpt_vstring(d1, "%-30s %d", "cur_uid:",                   accum->cur_uid);
   rpt_vstring(d1, "%-30s %s", "cur_user_in_group_i2c:",     bool_repr(accum->cur_user_in_group_i2c));
   rpt_vstring(d1, "%-30s %s", "cur_user_any_devi2c_rw:",    bool_repr(accum->cur_user_any_devi2c_rw));
   rpt_vstring(d1, "%-30s %s", "cur_user_all_devi2c_rw:",    bool_repr(accum->cur_user_all_devi2c_rw));

   if (accum->dev_i2c_device_numbers)
      free(dev_i2c_device_numbers_string);
   if (accum->sys_bus_i2c_device_numbers)
      free(sys_bus_i2c_device_numbers_string);
   if (accum->driver_list)
      free(driver_names);
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
Driver_Name_Node *
driver_name_list_find_exact(
      Driver_Name_Node * head,
      char *             driver_name)
{
   Driver_Name_Node * cur_node = head;
   while (cur_node && !streq(cur_node->driver_name, driver_name))
        cur_node = cur_node->next;
   return cur_node;
}


/** Checks if any driver name in the list of detected drivers starts with
 * the specified string.
 *
 *  \param  driver list     head of linked list of driver names
 *  \parar  driver_prefix   driver name prefix
 *  \return pointer to first node satisfying the prefix match, NULL if none
 */
Driver_Name_Node *
driver_name_list_find_prefix(
      Driver_Name_Node * head,
      char *             driver_prefix)
{
   Driver_Name_Node * curnode = head;
   while (curnode  && !str_starts_with(curnode->driver_name, driver_prefix) )
         curnode = curnode->next;
   // DBGMSG("driver_prefix=%s, returning %p", driver_prefix, curnode);
   return curnode;
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
   if (!driver_name_list_find_exact(*headptr, driver_name)) {
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
      closedir(d);
   }
}


/** Deletes lines from a #GPtrArray of text lines. If filter terms
 *  are specified, lines not satisfying any of the search terms are
 *  deleted.  Then, if **limit** is specified, at most the limit
 *  number of lines are left.
 *
 *  \param line_array   GPtrArray of null-terminated strings
 *  \param filter_terms null-terminated string array of terms
 *  \param ignore_case  if true, ignore case when testing filter terms
 *  \param  limit if 0, return all lines that pass filter terms
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
   bool debug = false;
   DBGMSF(debug, "line_array=%p, ct(filter_terms)=%d, ignore_case=%s, limit=%d",
            line_array, ntsa_length(filter_terms), bool_repr(ignore_case), limit);
   if (debug) {
      // (const char **) cast to conform to strjoin() signature
      char * s = strjoin( (const char **) filter_terms, -1, ", ");
      DBGMSG("Filter terms: %s", s);
      free(s);
   };
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
      // DBGMSF(debug, "s=|%s|", s);
      bool keep = true;
      if (filter_terms)
         keep = apply_filter_terms(s, filter_terms, ignore_case);
      if (!keep) {
         g_ptr_array_remove_index(line_array, ndx);
      }
   }
   gaux_ptr_array_truncate(line_array, limit);

   DBGMSF(debug, "Done. line_array->len=%d", line_array->len);
}


/** Reads the contents of a file into a #GPtrArray of lines, optionally keeping only
 *  those lines containing at least one on a list of terms.  After filtering, the set
 *  of returned lines may be further reduced to either the first or last n number of
 *  lines.
 *
 *  \param  line_array #GPtrArray in which to return the lines read
 *  \param  fn         file name
 *  \param  filter_terms  #Null_Terminated_String_Away of filter terms
 *  \param  ignore_case   ignore case when testing filter terms
 *  \param  limit if 0, return all lines that pass filter terms
 *                if > 0, return at most the first #limit lines that satisfy the filter terms
 *                if < 0, return at most the last  #limit lines that satisfy the filter terms
 *  \return if >= 0, number of lines before filtering and limit applied
 *          if < 0,  -errno
 *
 *  \remark
 *  This function was created because using grep in conjunction with pipes was
 *  producing obscure shell errors.
 */
int read_file_with_filter(
      GPtrArray * line_array,
      char *      fn,
      char **     filter_terms,
      bool        ignore_case,
      int         limit)
{
   bool debug = false;
   DBGMSF(debug, "line_array=%p, fn=%s, ct(filter_terms)=%d, ignore_case=%s, limit=%d",
            line_array, fn, ntsa_length(filter_terms), bool_repr(ignore_case), limit);

   g_ptr_array_set_free_func(line_array, g_free);    // in case not already set

   int rc = file_getlines(fn, line_array, /*verbose*/ true);
   DBGMSF(debug, "file_getlines() returned %d", rc);

   if (rc > 0) {
      filter_and_limit_g_ptr_array(
         line_array,
         filter_terms,
         ignore_case,
         limit);
   }
   else { // rc == 0
      DBGMSF(debug, "Empty file");
   }

   DBGMSF(debug, "Returning: %d", rc);
   return rc;
}


/** Execute a shell command and return the contents in a newly allocated
 *  #GPtrArray of lines. Optionally, keep only those lines containing at least
 *  one in a list of terms.  After filtering, the set of returned lines may
 *  be further reduced to either the first or last n number of lines.
 *
 *  \param  cmd        command to execute
 *  \param  fn         file name
 *  \param  filter_terms  #Null_Terminated_String_Away of filter terms
 *  \param  ignore_case   ignore case when testing filter terms
 *  \param  limit if 0, return all lines that pass filter terms
 *                if > 0, return at most the first #limit lines that satisfy the filter terms
 *                if < 0, return at most the last  #limit lines that satisfy the filter terms
 *  \param  result_loc  address at which to return a pointer to the newly allocate #GPtrArray
 *  \return if >= 0, number of lines before filtering and limit applied
 *          if < 0,  -errno
 */
int execute_cmd_collect_with_filter(
      char *       cmd,
      char **      filter_terms,
      bool         ignore_case,
      int          limit,
      GPtrArray ** result_loc)
{
   bool debug = false;
   DBGMSF(debug, "cmd|%s|, ct(filter_terms)=%d, ignore_case=%s, limit=%d",
            cmd, ntsa_length(filter_terms), bool_repr(ignore_case), limit);

   int rc = 0;
   GPtrArray *line_array = execute_shell_cmd_collect(cmd);
   if (!line_array) {
      rc = -1;
   }
   else {
      rc = line_array->len;
      if (rc > 0) {
         filter_and_limit_g_ptr_array(
            line_array,
            filter_terms,
            ignore_case,
            limit);
      }
   }
   *result_loc = line_array;

   DBGMSF(debug, "Returning: %d", rc);
   return rc;
}

