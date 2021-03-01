/** @file query_sysenv_base.c
 *
 * Base structures and functions for subsystem that diagnoses user configuration
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#define _GNU_SOURCE   // for asprintf() in stdio.h

/** \cond */
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>

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
      "ddc",
      NULL
};

static char * other_driver_modules[] = {
      "drm",
  //  "eeprom",       // not really interesting
      "i2c_algo_bit",
      "i2c_dev",
      "i2c_piix4",
      "ddcci",
      NULL
};

// in some contexts, module names are accepted with either underscores or hyphens
static char * other_driver_modules_alt[] = {
      "i2c-algo-bit",
      "i2c-dev",
      "i2c-piix4",
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

/** Returns a null terminated list of all driver strings of interest */
char ** get_all_driver_module_strings() {
   static char ** all_strings = NULL;
   if (!all_strings) {
      char ** all1 = ntsa_join(known_video_driver_modules, prefix_matches, false);
      char ** all2 = ntsa_join(all1, other_driver_modules, false);
      char ** all3 = ntsa_join(all2, other_driver_modules_alt, false);
      all_strings = all3;
      ntsa_free(all1, false);
      ntsa_free(all2, false);
   }
   return all_strings;
}


/** Reports the first line of a file, indented under a title.
 *  Issues a message if unable to read the file.
 *
 *  \param fn    file name
 *  \param title title message
 *  \param depth logical indentation depth
 */
void sysenv_rpt_file_first_line(const char * fn, const char * title, int depth) {
   int d1 = depth+1;
   if (title)
      rpt_title(title, depth);
   else
      rpt_vstring(depth, "%s:", fn);

   char * s = file_get_first_line(fn, true /* verbose */);
   if (s) {
      rpt_title(s, d1);
      free(s);
   }
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
bool sysenv_show_one_file(const char * dir_name, const char * simple_fn, bool verbose, int depth) {
   bool result = false;
   char * fqfn = g_strdup_printf("%s%s%s",
                                 dir_name,
                                 (str_ends_with(dir_name, "/")) ? "" : "/",
                                 simple_fn);
   if (regular_file_exists(fqfn)) {
      rpt_vstring(depth, "%s:", fqfn);
      rpt_file_contents(fqfn, /*verbose=*/true, depth+1);
      result = true;
   }
   else if (verbose)
      rpt_vstring(depth, "File not found: %s", fqfn);
   free(fqfn);
   return result;
}


/** Reports the current time as both local time and UTC time,
 *  and also the elapsed time in seconds since boot.
 *
 *  \param title  optional title
 *  \param depth  logical indentation depth
 */

void sysenv_rpt_current_time(const char * title, int depth)
{
   int d = depth;
   if (title) {
      rpt_title(title, depth);
      d = depth+1;
   }

   char buf0[80];
   time_t rawtime;
   struct tm * timeinfo;

   time ( &rawtime );
   timeinfo = localtime( &rawtime );
   strftime(buf0, 80, "%F %H:%M:%S %Z", timeinfo);
   rpt_vstring(d, "Current time (local): %s", buf0);

   timeinfo = gmtime( &rawtime );
   strftime(buf0, 80, "%F %H:%M:%S", timeinfo);
   rpt_vstring(d, "Current time (UTC):   %s", buf0);

   struct sysinfo info;
   sysinfo(&info);
   rpt_vstring(d, "Seconds since boot:   %ld", info.uptime);
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
   rpt_vstring(d1, "%-30s %s", "sysfs_i2c_devices_exist:",   sbool(accum->sysfs_i2c_devices_exist));
   rpt_vstring(d1, "%-30s %s", "/sys/bus/i2c device numbers:", sys_bus_i2c_device_numbers_string);
   rpt_vstring(d1, "%-30s %s", "dev_i2c_devices_required:",  sbool(accum->dev_i2c_devices_required));
   rpt_vstring(d1, "%-30s %s", "module_i2c_dev_needed:",     sbool(accum->module_i2c_dev_needed));
   rpt_vstring(d1, "%-30s %s", "module_i2c_dev_builtin:",    sbool(accum->module_i2c_dev_builtin));
   rpt_vstring(d1, "%-30s %s", "loadable_i2c_dev_exists:",   sbool(accum->loadable_i2c_dev_exists));
   rpt_vstring(d1, "%-30s %s", "i2c_dev_loaded_or_builtin:", sbool(accum->i2c_dev_loaded_or_builtin));
   rpt_vstring(d1, "%-30s %s", "group_i2c_checked:",         sbool(accum->group_i2c_checked));
   rpt_vstring(d1, "%-30s %s", "group_i2c_exists:",          sbool(accum->group_i2c_exists));
   rpt_vstring(d1, "%-30s %s", "dev_i2c_common_group_name:", accum->dev_i2c_common_group_name);
   rpt_vstring(d1, "%-30s %s", "all_dev_i2c_has_group_i2c:", sbool(accum->all_dev_i2c_has_group_i2c));
   rpt_vstring(d1, "%-30s %s", "any_dev_i2c_has_group_i2c:", sbool(accum->any_dev_i2c_has_group_i2c));
   rpt_vstring(d1, "%-30s %s", "all_dev_i2c_is_group_rw:",   sbool(accum->all_dev_i2c_is_group_rw));
   rpt_vstring(d1, "%-30s %s", "any_dev_i2c_is_group_rw:",   sbool(accum->any_dev_i2c_is_group_rw));
   rpt_vstring(d1, "%-30s %s", "cur_uname:",                 accum->cur_uname);
   rpt_vstring(d1, "%-30s %d", "cur_uid:",                   accum->cur_uid);
   rpt_vstring(d1, "%-30s %s", "cur_user_in_group_i2c:",     sbool(accum->cur_user_in_group_i2c));
   rpt_vstring(d1, "%-30s %s", "cur_user_any_devi2c_rw:",    sbool(accum->cur_user_any_devi2c_rw));
   rpt_vstring(d1, "%-30s %s", "cur_user_all_devi2c_rw:",    sbool(accum->cur_user_all_devi2c_rw));

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
      const char *             driver_name)
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
      const char *             driver_prefix)
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
void driver_name_list_add(Driver_Name_Node ** headptr, const char * driver_name) {
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


/** Given a path whose final segment is of the form "i2c-n",
 *  returns the bus number.
 *
 *  \param path  fully qualified or simple path name
 *  \return  I2C bus number, -1 if cannot be sxtracted
 */
int  i2c_path_to_busno(char * path) {
   bool debug = false;

   int busno = -1;
   if (path) {
      char * lastslash = strrchr(path, '/');
      char * basename = (lastslash) ? lastslash+1 : path;
      // char * basename = basename(path);
      if (basename) {
         if (str_starts_with(basename, "i2c-")) {
            int ival = 0;
            if (str_to_int(basename+4, &ival, 10) )
               busno = ival;
         }
      }
   }

   DBGMSF(debug, "path=%s, returning: %d", path, busno);
   return busno;
}

#ifdef SYSENV_TEST_IDENTICAL_EDIDS
// For testing situation  where 2 displays have the same EDID, e.g. LG displays
Byte * first_edid = NULL;
#endif

