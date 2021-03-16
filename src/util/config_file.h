/** config_file.h
 *  Read ddcutil configuration file
 */

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CONFIG_FILE_H_
#define CONFIG_FILE_H_

#include <stdbool.h>
#include <glib-2.0/glib.h>

#define PARSED_INI_FILE_MARKER "INIF"
typedef struct {
   char          marker[4];
   char *        config_fn;
   GHashTable *  hash_table;
} Parsed_Ini_File;

int    ini_file_load(
           const char *      ini_filename,
           GPtrArray*        errmsgs,
           bool              verbose,
           Parsed_Ini_File** ini_file_loc);
char * ini_file_get_value(
           Parsed_Ini_File * ini_file,
           const char *      segment,
           const char *      id);
void   ini_file_dump(
           Parsed_Ini_File * ini_file);
void   ini_file_free(
           Parsed_Ini_File * parsed_ini_file);
#endif /* CONFIG_FILE_H_ */
