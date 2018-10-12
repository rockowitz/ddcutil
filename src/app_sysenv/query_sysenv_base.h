/** @file query_sysenv_base.h
 *
 * Base structures and functions for subsystem that diagnoses user configuration
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef QUERY_SYSENV_BASE_H_
#define QUERY_SYSENV_BASE_H_

/** \cond */
#include <stdbool.h>
#include <glib-2.0/glib.h>

#include "util/data_structures.h"
/** \endcond */


char ** get_known_video_driver_module_names();
char ** get_prefix_match_names();
char ** get_other_driver_module_names();

void sysenv_rpt_file_first_line(char * fn, char * title, int depth);
bool sysenv_show_one_file(char * dir_name, char * simple_fn, bool verbose, int depth);
void sysenv_rpt_current_time(const char * title, int depth);

/** Linked list of names of detected drivers */
typedef struct driver_name_node {
   char * driver_name;
   struct driver_name_node * next;
} Driver_Name_Node;

Driver_Name_Node * driver_name_list_find_exact( Driver_Name_Node * head, char * driver_name);
Driver_Name_Node * driver_name_list_find_prefix(Driver_Name_Node * head, char * driver_prefix);
void driver_name_list_add(Driver_Name_Node ** headptr, char * driver_name);
void driver_name_list_free(Driver_Name_Node * driver_list);
char * driver_name_list_string(Driver_Name_Node * head);
bool only_fglrx(Driver_Name_Node * driver_list);
bool only_nvidia_or_fglrx(Driver_Name_Node * driver_list);


#define ENV_ACCUMULATOR_MARKER "ENVA"
/** Collects system environment information */
typedef struct {
   char               marker[4];
   char *             architecture;
   char *             distributor_id;
   bool               is_raspbian;
   bool               is_arm;
   Byte_Value_Array   dev_i2c_device_numbers;
   Driver_Name_Node * driver_list;
   bool               sysfs_i2c_devices_exist;
   Byte_Value_Array   sys_bus_i2c_device_numbers;
   bool               group_i2c_checked;
   bool               group_i2c_exists;
   bool               dev_i2c_devices_required;
   bool               all_dev_i2c_has_group_i2c;
   bool               any_dev_i2c_has_group_i2c;
   char *             dev_i2c_common_group_name;
   char *             cur_uname;
   uid_t              cur_uid;
   bool               cur_user_in_group_i2c;
   bool               cur_user_any_devi2c_rw;
   bool               cur_user_all_devi2c_rw;
   bool               module_i2c_dev_needed;
   bool               loadable_i2c_dev_exists;
   bool               module_i2c_dev_builtin;
   bool               i2c_dev_loaded_or_builtin;     // loaded or built-in
   bool               any_dev_i2c_is_group_rw;
   bool               all_dev_i2c_is_group_rw;
} Env_Accumulator;

Env_Accumulator * env_accumulator_new();
void env_accumulator_free(Env_Accumulator * accum);
void env_accumulator_report(Env_Accumulator * accum, int depth);


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

void filter_and_limit_g_ptr_array(
      GPtrArray * line_array,
      char **     filter_terms,
      bool        ignore_case,
      int         limit);

int read_file_with_filter(
      GPtrArray * line_array,
      char *      fn,
      char **     filter_terms,
      bool        ignore_case,
      int         limit);

int execute_cmd_collect_with_filter(
      char *       cmd,
      char **      filter_terms,
      bool         ignore_case,
      int          limit,
      GPtrArray ** result_loc);
#endif /* QUERY_SYSENV_BASE_H_ */
