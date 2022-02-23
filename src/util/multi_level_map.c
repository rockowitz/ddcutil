/* multi_level_map.c
 *
 * <copyright>
 * Copyright (C) 2015-2022 Sanford Rockowitz <rockowitz@minsoft.com>
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

/** @file multi_level_map.c
 * Multi_Level_Map data structure
 */

/** \cond */
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "report_util.h"

#include "multi_level_map.h"


//
// Data structure creation
//

/** Creates a new **Multi_Level_Map instance.
 *
 *  @param  table_name   name of table
 *  @param  levels       number of levels
 *  @param  level_detail pointer to array if **MLM_Level** descriptors
 */
Multi_Level_Map * mlm_create(char * table_name, int levels, MLM_Level* level_detail) {
   // printf("(%s) level_detail=%p\n", __func__, level_detail);
   // for (int lvlndx=0; lvlndx < levels; lvlndx++) {
   //    report_mlm_level(level_detail+lvlndx, 1);
   // }
   Multi_Level_Map * mlm =  calloc(1, sizeof(Multi_Level_Map) + levels * sizeof(MLM_Level));
   mlm->table_name = strdup(table_name);
   mlm->levels = levels;
   // MLM_Level* lvldesc = level_detail;
   int initial_size = level_detail[0].initial_size;
   // printf("(%s) initial_size=%d\n", __func__, initial_size);
   mlm->root = g_ptr_array_sized_new(initial_size);
   memcpy((Byte*) &mlm->level_detail, level_detail, levels*sizeof(MLM_Level));
   // report_multi_level_table(mlm,0);
   return mlm;
}


/** Adds a node to a **Multi_Level_Map**.
 *
 * @param map    pointer to **Multi_Level_Map** table
 * @param parent pointer to parent node
 *               if NULL, this node is a child of the root
 * @param key    key of node
 * @param value  value of node
 */
MLM_Node * mlm_add_node(Multi_Level_Map * map, MLM_Node * parent, uint key, char * value) {
   // printf("(%s) parent=%p, key=0x%04x, value=|%s|\n", __func__, parent, key, value);
   MLM_Node * new_node = calloc(1,sizeof(MLM_Node));
   new_node->code = key;
   new_node->name = value;
   new_node->children = NULL;

   if (!parent) {
      new_node->level = 0;
      g_ptr_array_add(map->root, new_node);
   }
   else {
      new_node->level = parent->level+1;
      if (!parent->children) {
         int initial_size = map->level_detail[parent->level].initial_size;
         parent->children = g_ptr_array_sized_new(initial_size);
      }
      g_ptr_array_add(parent->children, new_node);
   }
   map->level_detail[new_node->level].total_entries += 1;
   return new_node;
}


//
// Debug data structure
//

/** Reports on a **Multi_Level_Map** level descriptor.
 *  @param  level_desc pointer to level descriptor
 *  @param  depth      logical indentation depth
 */
void report_mlm_level(MLM_Level * level_desc, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("MLM_Level", level_desc, depth);
   rpt_str("name",          NULL, level_desc->name,          d1);
   rpt_int("initial_size",  NULL, level_desc->initial_size,  d1);
   rpt_int("total_entries", NULL, level_desc->total_entries, d1);
}


// Debugging function
void mlm_cur_entries(Multi_Level_Map * mlt) {
   int d1 = 1;
   rpt_vstring(0, "Multi_Level_Table.  levels=%d", mlt->levels);
   for (int ndx=0; ndx < mlt->levels; ndx++) {
      rpt_vstring(d1, "  mlt->level_detail[%d].cur_entry=%p, addr of entry=%p",
                      ndx, mlt->level_detail[ndx].cur_entry,  &mlt->level_detail[ndx].cur_entry);
   }
}


static
void report_mlm_node(
        Multi_Level_Map * header,
        int               level,
        MLM_Node *        entry,
        int               depth)
{
   // MLM_Level level_detail = header->level_detail[level];
   rpt_vstring(depth, "%04x  %s", entry->code, entry->name);
   if (entry->children) {
      for (int ndx=0; ndx<entry->children->len; ndx++) {
         report_mlm_node(
               header,
               level+1,
               g_ptr_array_index(entry->children, ndx),
               depth+1);
      }
   }
}


/** Reports the contents of a **Multi_Level_Map**.
 *
 * @param header pointer to MLM instance
 * @param depth  logical indentation depth
 */
void report_multi_level_map(Multi_Level_Map * header, int depth) {
   int d1 = depth+1;
   int d2 = depth+2;
   rpt_structure_loc("Multi_Level_Table", header, depth);
   rpt_vstring(d1, "%-20s:  %s", "Table",       header->table_name);
   rpt_vstring(d1, "%-20s:  %s", "Segment tag", header->segment_tag);
   rpt_int("Number of level 0 entries:", NULL, header->root->len, d1);

   for (int ndx=0; ndx < header->root->len; ndx++) {
      report_mlm_node(
            header,
            0,
            g_ptr_array_index(header->root, ndx),
            d2);
   }
}


//
// Data structure query
//

static
MLM_Node * mlm_find_child(GPtrArray * nodelist, uint id) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting, id=0x%08x\n", __func__, id);
   MLM_Node * result = NULL;

   for (int ndx = 0; ndx < nodelist->len; ndx++) {
      MLM_Node * cur_entry = g_ptr_array_index(nodelist, ndx);
      if (debug) printf("(%s) Comparing code=0x%04x, name=%s\n",
                 __func__, cur_entry->code, cur_entry->name);
      if (cur_entry->code == id) {
         result = cur_entry;
         break;
      }
   }

   if (debug)
      printf("(%s) Returning %p\n", __func__, (void*)result);
   return result;
}


static
void report_multi_level_names(Multi_Level_Names * mln, int depth) {
   int d1 = depth+1;
   rpt_structure_loc("Multi_Level_Names", mln, depth);
   rpt_int("levels", NULL, mln->levels, d1);
   for (int ndx = 0; ndx < mln->levels; ndx++) {
      rpt_str("names", NULL, mln->names[ndx], d1);
   }
}


/** Gets the names associated with the levels of a **Multi_Level_Map** path.
 *
 * @param mlm      pointer to **Multi_Level_Map** table
 * @param levelct  number of ids
 * @param ids      pointer to array of **levelct** node ids
 *
 * @return pointer to **Multi_Level_Names** struct containing the
 *         names for the ids at each level
 */
Multi_Level_Names mlm_get_names2(Multi_Level_Map * mlm, int levelct, uint* ids) {
   bool debug = false;
   assert(levelct >= 1 && levelct <= MLT_MAX_LEVELS);
   // pciusb_id_ensure_initialized();       // <==  WHAT TO DO?
   if (debug) {
      printf("(%s) levelct=%d\n", __func__, levelct);
      for (int ndx = 0; ndx < levelct; ndx++) {
         printf("     ids[%d] = 0x%08x\n", ndx, ids[ndx]);
      }
   }

   Multi_Level_Names result = {0};

   int argndx = 0;
   GPtrArray * children = mlm->root;
   result.levels = 0;
   while (argndx < levelct) {
      // printf("(%s) argndx=%d\n", __func__, argndx);
      if (!children)
         break;
      MLM_Node * level_entry = mlm_find_child(children, ids[argndx]);
      if (!level_entry) {
         break;
      }
      result.levels = argndx+1;
      result.names[argndx] = level_entry->name;
      children = level_entry->children;
      argndx++;
   }

   if (debug) {
      printf("(%s) Returning: \n", __func__);
      report_multi_level_names(&result, 1);
   }
   return result;
}


/** Variant of **mlm_get_names2()** that uses a variable argument list for
 *  the level ids.
 *
 * @param table    pointer  to **Multi_Level_Map** table
 * @param argct    number of ids
 * @param ..       node ids, of type uint
 *
 * @return pointer to **Multi_Level_Names** struct containing the
 *         names for the ids at each level
 */
Multi_Level_Names mlm_get_names(Multi_Level_Map * table, int argct, ...) {
  // bool debug = false;
  assert(argct >= 1 && argct <= MLT_MAX_LEVELS);
  // pciusb_id_ensure_initialized();       // <==  WHAT TO DO?

  uint args[MLT_MAX_LEVELS];

  va_list ap;
  int ndx;
  va_start(ap, argct);
  for (ndx=0; ndx<argct; ndx++) {
     args[ndx] = va_arg(ap,int);
     // printf("(%s) args[%d] = 0x%04x\n", __func__, ndx, args[ndx]);
  }
  va_end(ap);

  return mlm_get_names2(table, argct, args);
}


#ifdef OLD
// new implementation wrappers mlm_get_names2()
// Implement using varargs
// variable arguments are of type uint
Multi_Level_Names mlm_get_names_old(Multi_Level_Map * table, int argct, ...) {
  bool debug = false;
  assert(argct >= 1 && argct <= MLT_MAX_LEVELS);
  // pciusb_id_ensure_initialized();       // <==  WHAT TO DO?

  Multi_Level_Names result = {0};

  uint args[MLT_MAX_LEVELS];

  va_list ap;
  int ndx;
  va_start(ap, argct);
  for (ndx=0; ndx<argct; ndx++) {
     args[ndx] = va_arg(ap,int);
     // printf("(%s) args[%d] = 0x%04x\n", __func__, ndx, args[ndx]);
  }
  va_end(ap);

  int argndx = 0;
  GPtrArray * children = table->root;
  while (argndx < argct) {
     assert(children);
     MLM_Node * level_entry = mlm_find_child(children, args[argndx]);
     if (!level_entry) {
        result.levels = 0;   // indicates not found
        break;
     }
     result.levels = argndx+1;
     result.names[argndx] = level_entry->name;
     children = level_entry->children;
     argndx++;
  }

  if (debug) {
     printf("(%s) Returning: \n", __func__);
     report_multi_level_names(&result, 1);
  }
  return result;
}
#endif


