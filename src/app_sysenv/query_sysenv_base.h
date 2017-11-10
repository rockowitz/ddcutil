/* query_sysenv_base.h
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
 * Base structures and functions for subsystem that diagnoses user configuration
 */

#ifndef QUERY_SYSENV_BASE_H_
#define QUERY_SYSENV_BASE_H_

/** \cond */
#include <stdbool.h>

#include "util/data_structures.h"
/** \endcond */


char ** get_known_video_driver_module_names();
char ** get_prefix_match_names();
char ** get_other_driver_module_names();

void sysenv_rpt_file_first_line(char * fn, char * title, int depth);
bool sysenv_show_one_file(char * dir_name, char * simple_fn, bool verbose, int depth);

/** Linked list of names of detected drivers */
typedef struct driver_name_node {
   char * driver_name;
   struct driver_name_node * next;
} Driver_Name_Node;

Driver_Name_Node * driver_name_list_find(Driver_Name_Node * head, char * driver_name);
void driver_name_list_add(struct driver_name_node ** headptr, char * driver_name);
void driver_name_list_free(struct driver_name_node * driver_list);


#define ENV_ACCUMULATOR_MARKER "ENVA"

/** Collects system environment information */
typedef struct {
   char               marker[4];
   char *             architecture;
   char *             distributor_id;
   bool               is_raspbian;
   bool               is_arm;
   Byte_Value_Array   i2c_device_numbers;
   Driver_Name_Node * driver_list;
   bool               sysfs_i2c_devices_exist;
} Env_Accumulator;

Env_Accumulator * env_accumulator_new();
void env_accumulator_free(Env_Accumulator * accum);

/** Signature of filename filter function passed to #dir_foreach(). */
typedef bool (*Filename_Filter_Func)(char * simple_fn);

/** Signature of function called by #dir_foreach to process each file. */
typedef void (*Dir_Foreach_Func)(char * dirname, char * fn, void * accumulator, int depth);

void dir_foreach(
      char *               dirname,
      Filename_Filter_Func fn_filter,
      Dir_Foreach_Func     func,
      void *               accumulator,
      int                  depth);

#endif /* QUERY_SYSENV_BASE_H_ */
