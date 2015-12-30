/* vcp_feature_groups.c
 *
 * Created on: Dec 29, 2015
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <assert.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "ddc/ddc_services.h"      // TEMP, circular, while VCP_Feature_Subset defined here
#include "ddc/vcp_feature_groups.h"




#define VCP_FEATURE_GROUP_MARKER "VFGP"
struct VCP_Feature_Group {
   char         marker[4];
   GPtrArray *  members;
};


VCP_Feature_Group create_feature_group(VCP_Feature_Subset subset, Version_Spec vcp_version) {
   struct VCP_Feature_Group * fgrp = calloc(1,sizeof(struct VCP_Feature_Group));
   memcpy(fgrp->marker, VCP_FEATURE_GROUP_MARKER, 4);
   fgrp->members = g_ptr_array_sized_new(30);
   if (subset == SUBSET_SCAN) {
      int ndx = 0;
      for (ndx = 0; ndx < 256; ndx++) {
         Byte id = ndx;
         VCP_Feature_Table_Entry* vcp_entry = vcp_find_feature_by_hexid_w_default(id);
         // original code looks at VCP2_READABLE, output level
         g_ptr_array_add(fgrp->members, vcp_entry);
      }

   }
   else {
      int known_feature_ct = vcp_get_feature_code_count();
      int ndx = 0;
      for (ndx=0; ndx < known_feature_ct; ndx++) {
         VCP_Feature_Table_Entry * vcp_entry = vcp_get_feature_table_entry(ndx);
         assert(vcp_entry);
         Version_Feature_Flags vflags = 0;
         bool showit = false;
         switch(subset) {
         case SUBSET_ALL:
            showit = true;
            break;
         case SUBSET_SUPPORTED:
            showit = true;
            break;
         case SUBSET_COLORMGT:
            vflags = get_version_specific_feature_flags(vcp_entry, vcp_version);;
            showit = vflags & VCP2_COLORMGT;
            break;
         case SUBSET_PROFILE:
            vflags = get_version_specific_feature_flags(vcp_entry, vcp_version);;
            showit = vflags & VCP2_PROFILE;
            break;
         case SUBSET_SCAN:    // will never happen, inserted to avoid compiler warning
            break;
         }
         if (showit) {
            g_ptr_array_add(fgrp->members, vcp_entry);
         }
      }
   }

   return fgrp;
}


VCP_Feature_Group create_single_feature_group_by_vcp_entry(VCP_Feature_Table_Entry * vcp_entry) {
   struct VCP_Feature_Group * fgrp = calloc(1,sizeof(struct VCP_Feature_Group));
   memcpy(fgrp->marker, VCP_FEATURE_GROUP_MARKER, 4);
   fgrp->members = g_ptr_array_sized_new(1);
   g_ptr_array_add(fgrp->members, vcp_entry);
   return fgrp;
}


VCP_Feature_Group create_single_feature_group_by_hexid(Byte id, bool force) {
   struct VCP_Feature_Group * fgrp = NULL;
   VCP_Feature_Table_Entry* vcp_entry = NULL;
   if (force)
      vcp_entry = vcp_find_feature_by_hexid_w_default(id);
   else
      vcp_entry = vcp_find_feature_by_hexid(id);
   if (vcp_entry)
      fgrp = create_single_feature_group_by_vcp_entry(vcp_entry);
   return fgrp;
}


VCP_Feature_Group create_single_feature_group_by_charid(Byte id, bool force) {
   // TODO: copy and modify existing code:
   return NULL;
}


void free_feature_group(VCP_Feature_Group feature_group) {
   struct VCP_Feature_Group * fgrp = (struct VCP_Feature_Group *) feature_group;
   assert( fgrp && memcmp(fgrp->marker, VCP_FEATURE_GROUP_MARKER, 4) == 0);
   int ndx = 0;
   // free all generated members
   for (; ndx < fgrp->members->len; ndx++) {
      VCP_Feature_Table_Entry * vcp_entry = NULL;
      vcp_entry = g_ptr_array_index(fgrp->members,ndx);
      if (vcp_entry->vcp_global_flags & VCP2_SYNTHETIC) {
         // free_vcp_feature_table_entry(vcp_entry);    // UNIMPLEMENTED
      }
   }
   fgrp->marker[3] = 'x';
   free(fgrp);
}

VCP_Feature_Table_Entry * get_feature_group_entry(VCP_Feature_Group feature_group, int index) {
   struct VCP_Feature_Group * fgrp = (struct VCP_Feature_Group *) feature_group;
   assert( fgrp && memcmp(fgrp->marker, VCP_FEATURE_GROUP_MARKER, 4) == 0);
   VCP_Feature_Table_Entry * ventry = NULL;
   if (index >= 0 || index < fgrp->members->len)
      ventry = g_ptr_array_index(fgrp->members,index);
   return ventry;
}

void report_feature_group(VCP_Feature_Group feature_group, int depth) {
   struct VCP_Feature_Group * fgrp = (struct VCP_Feature_Group *) feature_group;
   assert( fgrp && memcmp(fgrp->marker, VCP_FEATURE_GROUP_MARKER, 4) == 0);
   int ndx = 0;
   // free all generated members
   for (; ndx < fgrp->members->len; ndx++) {
      VCP_Feature_Table_Entry * vcp_entry = NULL;
      vcp_entry = g_ptr_array_index(fgrp->members,ndx);
      // TODO: replace w rpt function that uses depth:
      printf("VCP code: 0x%02x: %s\n",
             vcp_entry->code,
             get_non_version_specific_feature_name(vcp_entry)
            );
   }
}
