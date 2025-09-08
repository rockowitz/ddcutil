/** \file simple_ini_file.h
 *
 *  Reads an INI style configuration file
 */

// Copyright (C) 2021-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SIMPLE_INI_FILE_H_
#define SIMPLE_INI_FILE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <glib-2.0/glib.h>

#include "error_info.h"
#include "string_util.h"

#define PARSED_INI_FILE_MARKER "INIF"
typedef struct {
   char          marker[4];
   char *        config_fn;
   GHashTable *  hash_table;
} Parsed_Ini_File;

typedef struct  {
   char * segment_name;
   char * key_name;
} Ini_Valid_Section_Key_Pairs;

int    ini_file_load(
           const char *      ini_filename,
           Ini_Valid_Section_Key_Pairs   valid_segment_key_pairs[],
           int               valid_segment_key_pair_ct,
           GPtrArray*        errmsgs,
           Parsed_Ini_File** ini_file_loc);

#ifdef UNUSED

bool ini_file_validate(Parsed_Ini_File *            parsed_ini_file,
                       Ini_Valid_Section_Key_Pairs   valid_segment_key_pairs[],
                       int                          kvp_ct,
                       GPtrArray *                  errmsgs);
#endif

char * ini_file_get_value(
           Parsed_Ini_File * ini_file,
           const char *      segment,
           const char *      id);

void   ini_file_dump(
           Parsed_Ini_File * ini_file);

void   ini_file_free(
           Parsed_Ini_File * parsed_ini_file);

#ifdef __cplusplus
}    // extern "C"
#endif

#endif /* SIMPLE_INI_FILE_H_ */
