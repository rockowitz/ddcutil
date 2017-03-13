/* multi_level_map.h
 *
 * <copyright>
 * Copyright (C) 2015-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** @file multi_level_map.h
 * Multi_Level_Map data structure
 */

#ifndef MULTI_LEVEL_TABLE_H_
#define MULTI_LEVEL_TABLE_H_

/** \cond */
#include <glib.h>
/** \endcond */


#define MLT_MAX_LEVELS 4

typedef struct {
   int   levels;
   char * names[MLT_MAX_LEVELS];
} Multi_Level_Names;

typedef struct {
   int   levels;
   uint  ids[MLT_MAX_LEVELS];
} Multi_Level_Ids;


typedef struct {
   ushort   level;
   uint     code;
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
MLM_Node * mlm_add_node(Multi_Level_Map * mlm, MLM_Node * parent, uint key, char * value);

void report_multi_level_map(Multi_Level_Map * mlm, int depth);

Multi_Level_Names mlm_get_names(Multi_Level_Map * mlm, int argct, ...);

Multi_Level_Names mlm_get_names2(Multi_Level_Map * mlm, int levelct, uint* ids);


#endif /* MULTI_LEVEL_TABLE_H_ */
