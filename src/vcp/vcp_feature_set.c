/* vcp_feature_set.c
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/report_util.h"

#include "base/core.h"

#include "vcp/vcp_feature_set.h"


#define VCP_FEATURE_SET_MARKER "FSET"
struct vcp_feature_set {
   char                marker[4];
   VCP_Feature_Subset  subset;
   GPtrArray *         members;
};



/* Free only synthetic VCP_Feature_Table_Entrys,
 * not ones in the permanent data structure.
 */
void free_transient_vcp_entry(gpointer ptr) {
   // DBGMSG("Starting.  ptr=%p", ptr);
   assert(ptr);

   VCP_Feature_Table_Entry * pfte = (VCP_Feature_Table_Entry *) ptr;
   // DBGMSG("pfte=%p, marker = %.4s   %s", pfte, pfte->marker, hexstring(pfte->marker,4));
   assert(memcmp(pfte->marker, VCP_FEATURE_TABLE_ENTRY_MARKER, 4) == 0);
   if (pfte->vcp_global_flags & DDCA_SYNTHETIC) {
      free_synthetic_vcp_entry(pfte);
   }
}


void free_vcp_feature_set(VCP_Feature_Set fset) {
   if (fset) {
      struct vcp_feature_set * pset = (struct vcp_feature_set *) fset;
      assert( memcmp(pset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);

      if (pset->members) {
         g_ptr_array_set_free_func(pset->members, free_transient_vcp_entry);
         g_ptr_array_free(pset->members, true);
      }
      free(pset);
   }
}

VCP_Feature_Set
create_feature_set(VCP_Feature_Subset subset_id, DDCA_MCCS_Version_Spec vcp_version) {
   assert(subset_id);
   bool debug = false;
   DBGMSF(debug, "Starting. subset_id=%s(0x%04x), vcp_version=%d.%d",
                 feature_subset_name(subset_id), subset_id, vcp_version.major, vcp_version.minor);
   struct vcp_feature_set * fset = calloc(1,sizeof(struct vcp_feature_set));
   memcpy(fset->marker, VCP_FEATURE_SET_MARKER, 4);
   fset->subset = subset_id;
   fset->members = g_ptr_array_sized_new(30);
   if (subset_id == VCP_SUBSET_SCAN || subset_id == VCP_SUBSET_MFG) {
      int ndx = 0;
      if (subset_id == VCP_SUBSET_MFG)
         ndx = 0xe0;
      for (; ndx < 256; ndx++) {
         Byte id = ndx;
         // DBGMSF(debug, "examining id 0x%02x", id);
         // n. this is a pointer into permanent data structures, should not be freed:
         VCP_Feature_Table_Entry* vcp_entry = vcp_find_feature_by_hexid(id);
         // original code looks at VCP2_READABLE, output level
         if (vcp_entry)
            g_ptr_array_add(fset->members, vcp_entry);
         else {
            g_ptr_array_add(fset->members, vcp_create_dummy_feature_for_hexid(id));
            if (ndx >= 0xe0 && (get_output_level() >= DDCA_OL_VERBOSE) ) {
               // for manufacturer specific features, probe as both table and non-table
               // Only probe table if --verbose, output is confusing otherwise
               g_ptr_array_add(fset->members, vcp_create_table_dummy_feature_for_hexid(id));
            }
         }
      }
   }
   else {
      int known_feature_ct = vcp_get_feature_code_count();
      int ndx = 0;
      for (ndx=0; ndx < known_feature_ct; ndx++) {
         VCP_Feature_Table_Entry * vcp_entry = vcp_get_feature_table_entry(ndx);
         assert(vcp_entry);
         DDCA_Version_Feature_Flags vflags = 0;
         bool showit = false;
         switch(subset_id) {
         case VCP_SUBSET_PRESET:
            showit = vcp_entry->vcp_spec_groups & VCP_SPEC_PRESET;
            break;
         case VCP_SUBSET_TABLE:
            vflags = get_version_specific_feature_flags(vcp_entry, vcp_version);
            showit = vflags & DDCA_TABLE;
            break;
         case VCP_SUBSET_KNOWN:
         case VCP_SUBSET_ALL:
         case VCP_SUBSET_SUPPORTED:
            showit = true;
            break;
         case VCP_SUBSET_COLOR:
         case VCP_SUBSET_PROFILE:
         case VCP_SUBSET_LUT:
         case VCP_SUBSET_TV:
         case VCP_SUBSET_AUDIO:
         case VCP_SUBSET_WINDOW:
         case VCP_SUBSET_DPVL:
         case VCP_SUBSET_CRT:
            showit = vcp_entry->vcp_subsets & subset_id;
            break;
         case VCP_SUBSET_SCAN:    // will never happen, inserted to avoid compiler warning
         case VCP_SUBSET_MFG:
         case VCP_SUBSET_SINGLE_FEATURE:
         case VCP_SUBSET_NONE:
            break;
         }
         if (showit) {
            g_ptr_array_add(fset->members, vcp_entry);
         }
      }
   }

   return fset;
}



VCP_Feature_Set
create_single_feature_set_by_vcp_entry(VCP_Feature_Table_Entry * vcp_entry) {
   // bool debug = true;
   struct vcp_feature_set * fset = calloc(1,sizeof(struct vcp_feature_set));
   memcpy(fset->marker, VCP_FEATURE_SET_MARKER, 4);
   fset->members = g_ptr_array_sized_new(1);
   fset->subset = VCP_SUBSET_SINGLE_FEATURE;
   g_ptr_array_add(fset->members, vcp_entry);
   return fset;
}

/* Creates a VCP_Feature_Set for a single VCP code
 *
 * Arguments:
 *    id      feature id
 *    force   indicates behavior if feature id not found in vcp_feature_table,
 *            if true, creates a feature set using a dummy feature table entry
 *            if false, returns NULL
 *
 * Returns: feature set containing a single feature
 *          NULL if the feature not found and force not specified
 */
VCP_Feature_Set
create_single_feature_set_by_hexid(Byte id, bool force) {
   struct vcp_feature_set * fset = NULL;
   VCP_Feature_Table_Entry* vcp_entry = NULL;
   if (force)
      vcp_entry = vcp_find_feature_by_hexid_w_default(id);
   else
      vcp_entry = vcp_find_feature_by_hexid(id);
   if (vcp_entry)
      fset = create_single_feature_set_by_vcp_entry(vcp_entry);
   return fset;
}


/* Creates a VCP_Feature_Set from an external feature specification
 *
 * Arguments:
 *    fsref   external feature set descriptor
 *    force   indicates behavior in the case of a single feature code
 *            if the feature id is not found in vcp_feature_table,
 *            if true, creates a feature set using a dummy feature table entry
 *            if false, returns NULL
 *
 * Returns: feature set containing a single feature
 *          NULL if the feature not found and force not specified
 */
VCP_Feature_Set
create_feature_set_from_feature_set_ref(
   Feature_Set_Ref * fsref,
   DDCA_MCCS_Version_Spec      vcp_version,
   bool              force)
{
    struct vcp_feature_set * fset = NULL;
    if (fsref->subset == VCP_SUBSET_SINGLE_FEATURE)
       fset = create_single_feature_set_by_hexid(fsref->specific_feature, force);
    else
       fset = create_feature_set(fsref->subset, vcp_version);
    return fset;
}


VCP_Feature_Set create_single_feature_set_by_charid(Byte id, bool force) {
   // TODO: copy and modify existing code:
   return NULL;
}

static inline struct vcp_feature_set *
unopaque_feature_set(VCP_Feature_Set feature_set) {
   struct vcp_feature_set * fset = (struct vcp_feature_set *) feature_set;
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   return fset;
}


void free_feature_set(VCP_Feature_Set feature_set) {
   struct vcp_feature_set * fset = (struct vcp_feature_set *) feature_set;
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   int ndx = 0;
   // free all generated members
   for (; ndx < fset->members->len; ndx++) {
      VCP_Feature_Table_Entry * vcp_entry = NULL;
      vcp_entry = g_ptr_array_index(fset->members,ndx);
      if (vcp_entry->vcp_global_flags & DDCA_SYNTHETIC) {
         // free_vcp_feature_table_entry(vcp_entry);    // UNIMPLEMENTED
      }
   }
   fset->marker[3] = 'x';
   free(fset);
}

VCP_Feature_Table_Entry * get_feature_set_entry(VCP_Feature_Set feature_set, int index) {
   struct vcp_feature_set * fset = (struct vcp_feature_set *) feature_set;
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   VCP_Feature_Table_Entry * ventry = NULL;
   if (index >= 0 || index < fset->members->len)
      ventry = g_ptr_array_index(fset->members,index);
   return ventry;
}

int get_feature_set_size(VCP_Feature_Set feature_set) {
   struct vcp_feature_set * fset = (struct vcp_feature_set *) feature_set;
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   return fset->members->len;
}

VCP_Feature_Subset get_feature_set_subset_id(VCP_Feature_Set feature_set) {
   struct vcp_feature_set * fset = (struct vcp_feature_set *) feature_set;
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   return fset->subset;
}

void report_feature_set(VCP_Feature_Set feature_set, int depth) {
   struct vcp_feature_set * fset = (struct vcp_feature_set *) feature_set;
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   int ndx = 0;
   for (; ndx < fset->members->len; ndx++) {
      VCP_Feature_Table_Entry * vcp_entry = NULL;
      vcp_entry = g_ptr_array_index(fset->members,ndx);
      rpt_vstring(depth,
                  "VCP code: %02X: %s",
                  vcp_entry->code,
                  get_non_version_specific_feature_name(vcp_entry)
                 );
   }
}

void filter_feature_set(VCP_Feature_Set feature_set, VCP_Feature_Set_Filter_Func func) {
   bool debug = true;

   struct vcp_feature_set * fset = (struct vcp_feature_set *) feature_set;
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);

   for (int ndx = fset->members->len -1; ndx >= 0; ndx--) {
      VCP_Feature_Table_Entry * vcp_entry = NULL;
      vcp_entry = g_ptr_array_index(fset->members,ndx);
      if (!func(vcp_entry)) {
         DBGMSF(debug, "Removing entry");
         // memory leak?
         g_ptr_array_remove_index(fset->members, ndx);
         if (vcp_entry->vcp_global_flags & DDCA_SYNTHETIC) {
            free_synthetic_vcp_entry(vcp_entry);
         }

      }

   }
}
