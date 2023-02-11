/** @file multi_level_map.h */

// Copyright (C) 2021-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** @file multi_level_map.h
 * Multi_Level_Map data structure
 */

#ifndef MULTI_LEVEL_TABLE_H_
#define MULTI_LEVEL_TABLE_H_

/** \cond */
#include <glib-2.0/glib.h>
/** \endcond */

#define MLT_MAX_LEVELS 4

typedef struct {
   int   levels;
   char * names[MLT_MAX_LEVELS];
} Multi_Level_Names;

typedef struct {
   int   levels;
   guint  ids[MLT_MAX_LEVELS];
} Multi_Level_Ids;


typedef struct {
   gushort   level;
   guint     code;
   char *   name;
   GPtrArray * children;
} MLM_Node;

/* Used to both describe a level in a **Multi_Level_Map** table,
 * /and maintain data about that level
 */
typedef struct {
   char *      name;
   int         initial_size;
   int         total_entries;
   MLM_Node *  cur_entry;
} MLM_Level;

void report_mlm_level(MLM_Level * level_desc, int depth);

typedef struct {
   char*       table_name;
   char*       segment_tag;
   int         levels;
   GPtrArray * root;
   MLM_Level   level_detail[];
   // MLM_Level * level_detail;
} Multi_Level_Map;


Multi_Level_Map * mlm_create(char * table_name, int levels, MLM_Level* level_detail);
MLM_Node * mlm_add_node(Multi_Level_Map * mlm, MLM_Node * parent, guint key, char * value);

void report_multi_level_map(Multi_Level_Map * mlm, int depth);

Multi_Level_Names mlm_get_names(Multi_Level_Map * mlm, int argct, ...);

Multi_Level_Names mlm_get_names2(Multi_Level_Map * mlm, int levelct, guint* ids);

#endif /* MULTI_LEVEL_TABLE_H_ */
