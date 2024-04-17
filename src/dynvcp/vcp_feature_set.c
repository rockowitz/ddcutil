/** @file vcp_feature_set.c
 */

// Copyright (C) 2014-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/debug_util.h"
#include "util/report_util.h"
/** \endcond */

#include "base/core.h"
#include "base/rtti.h"

#include "dynvcp/vcp_feature_set.h"

// Default trace class for this file
// static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_VCP;


/* Free only synthetic VCP_Feature_Table_Entrys,
 * not ones in the permanent data structure.
 */
void free_transient_vcp_entry(gpointer ptr) {
   // DBGMSG("Starting.  ptr=%p", ptr);
   assert(ptr);

   VCP_Feature_Table_Entry * pfte = (VCP_Feature_Table_Entry *) ptr;
   // DBGMSG("pfte=%p, marker = %.4s   %s", pfte, pfte->marker, hexstring(pfte->marker,4));
   assert(memcmp(pfte->marker, VCP_FEATURE_TABLE_ENTRY_MARKER, 4) == 0);
   if (pfte->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY) {
      free_synthetic_vcp_entry(pfte);
   }
}




void free_vcp_feature_set(VCP_Feature_Set * pset) {
   if (pset) {
      assert( memcmp(pset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);

      if (pset->members) {
         g_ptr_array_set_free_func(pset->members, free_transient_vcp_entry);
         g_ptr_array_free(pset->members, true);
      }
      free(pset);
   }
}




VCP_Feature_Table_Entry *
get_vcp_feature_set_entry(
      VCP_Feature_Set * fset,
      unsigned          index)
{
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   VCP_Feature_Table_Entry * ventry = NULL;
   if (index < fset->members->len)  // n. index is unsigned, no need to test if >= 0
      ventry = g_ptr_array_index(fset->members,index);
   return ventry;
}


int get_vcp_feature_set_size(VCP_Feature_Set * fset) {
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   return fset->members->len;
}


void report_vcp_feature_set(VCP_Feature_Set* fset, int depth) {
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


void dbgrpt_vcp_feature_set(VCP_Feature_Set* fset, int depth) {
   rpt_structure_loc("VCP_Feature_Set",fset,depth);
   int d1 = depth+1;
   int d2 = depth+2;

   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   rpt_vstring(depth, "Subset: %d (%s)", fset->subset, feature_subset_name(fset->subset));
   if (fset->members->len == 0)
      rpt_label(d1, "No members");
   else {
      for (int ndx = 0; ndx < fset->members->len; ndx++) {
         VCP_Feature_Table_Entry * vcp_entry = g_ptr_array_index(fset->members,ndx);
         rpt_vstring(d1,
                     "VCP code: %02X: %s",
                     vcp_entry->code,
                     get_non_version_specific_feature_name(vcp_entry)
                    );
         char buf[50];
         rpt_vstring(d2, "Global feature flags: 0x%04x - %s",
                         vcp_entry->vcp_global_flags,
                         vcp_interpret_global_feature_flags(vcp_entry->vcp_global_flags, buf, 50) );
      }
   }
}


#ifdef OLD
VCP_Feature_Set *
create_feature_set0(
      VCP_Feature_Subset   subset_id,
      GPtrArray *          members)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting. subset_id=%d, number of members=%d",
                              subset_id, (members) ? members->len : -1);

   struct vcp_feature_set * fset = calloc(1,sizeof(struct vcp_feature_set));
   memcpy(fset->marker, VCP_FEATURE_SET_MARKER, 4);
   fset->subset = subset_id;
   fset->members = members;

   DBGTRC(debug, TRACE_GROUP, "Returning %p", fset);
   return fset;
}
#endif

#ifdef UNUSED
// used only for VCPINFO
VCP_Feature_Set *
create_single_feature_set_by_vcp_entry(VCP_Feature_Table_Entry * vcp_entry) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "vcp_entry=%p", vcp_entry);

   struct vcp_feature_set * fset = calloc(1,sizeof(struct vcp_feature_set));
   assert(fset);     // avoid coverity "Dereference before null check" warning
   memcpy(fset->marker, VCP_FEATURE_SET_MARKER, 4);
   fset->members = g_ptr_array_sized_new(1);
   fset->subset = VCP_SUBSET_SINGLE_FEATURE;
   g_ptr_array_add(fset->members, vcp_entry);

   if (debug || IS_TRACING()) {
      DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %p", fset);
      if (fset)
         dbgrpt_vcp_feature_set(fset, 1);
   }
   return fset;
}
#endif

#ifdef UNUSED
/* Creates a VCP_Feature_Set for a single VCP code
 *
 * \param  id      feature id
 * \param  force   indicates behavior if feature id not found in vcp_feature_table,
 *                 - if true, creates a feature set using a dummy feature table entry
 *                 - if false, returns NULL
 * \return  feature set containing a single feature,
 *          NULL if the feature not found and force not specified
 *
 * \remark used only for VCPINFO
 */
VCP_Feature_Set *
create_single_feature_set_by_hexid(Byte id, bool force) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "id=0x%02x, force=%s", id, sbool(force));

   struct vcp_feature_set * fset = NULL;
   VCP_Feature_Table_Entry* vcp_entry = NULL;
   if (force)
      vcp_entry = vcp_find_feature_by_hexid_w_default(id);
   else
      vcp_entry = vcp_find_feature_by_hexid(id);
   if (vcp_entry)
      fset = create_single_feature_set_by_vcp_entry(vcp_entry);

   if (debug || IS_TRACING()) {
      DBGTRC_DONE(debug, TRACE_GROUP, "Returning: %p", fset);
      if (fset)
         dbgrpt_vcp_feature_set(fset, 1);
   }
   return fset;
}
#endif


#ifdef OLD
static inline struct vcp_feature_set *
unopaque_feature_set(
      VCP_Feature_Set feature_set)
{
   struct vcp_feature_set * fset = (struct vcp_feature_set *) feature_set;
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   return fset;
}
#endif

#ifdef UNUSED
void free_feature_set(VCP_Feature_Set * fset) {
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   int ndx = 0;
   // free all generated members
   for (; ndx < fset->members->len; ndx++) {
      VCP_Feature_Table_Entry * vcp_entry = NULL;
      vcp_entry = g_ptr_array_index(fset->members,ndx);
      if (vcp_entry->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY) {
         free_synthetic_vcp_entry(vcp_entry);
      }
   }
   fset->marker[3] = 'x';
   free(fset);
}
#endif

#ifdef UNUSED
void replace_vcp_feature_set_entry(
      VCP_Feature_Set * fset,
      unsigned          index,
      VCP_Feature_Table_Entry * new_entry)
{
   assert(fset);
   assert(new_entry);
   assert (index < fset->members->len);
   if (index < fset->members->len) {
      VCP_Feature_Table_Entry * old_entry = g_ptr_array_index(fset->members, index);
      g_ptr_array_remove_index(fset->members, index);
      g_ptr_array_insert(fset->members, index, new_entry);
      if (old_entry->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY) {
         free_synthetic_vcp_entry(old_entry);
      }
   }
}
#endif

#ifdef UNUSED
VCP_Feature_Subset get_feature_set_subset_id(VCP_Feature_Set* fset) {
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   return fset->subset;
}
#endif


#ifdef UNUSED
void
filter_feature_set(
      VCP_Feature_Set *            fset,
      VCP_Feature_Set_Filter_Func  func)
{
   bool debug = false;

   assert(fset);

   for (int ndx = fset->members->len -1; ndx >= 0; ndx--) {
      VCP_Feature_Table_Entry * vcp_entry = NULL;
      vcp_entry = g_ptr_array_index(fset->members,ndx);
      if (!func(vcp_entry)) {
         DBGMSF(debug, "Removing entry");
         // memory leak?
         g_ptr_array_remove_index(fset->members, ndx);
         if (vcp_entry->vcp_global_flags & DDCA_SYNTHETIC_VCP_FEATURE_TABLE_ENTRY) {
            free_synthetic_vcp_entry(vcp_entry);
         }
      }
   }
}
#endif


#ifdef UNUSED   // replaced by feature_list_from_dyn_feature_set()
// or, take DDCA_Feature_List address as parm
DDCA_Feature_List
feature_list_from_feature_set(VCP_Feature_Set * fset)
{
   bool debug = false;
   if (debug || IS_TRACING()) {
      DBGMSG("Starting. feature_set = %p -> %s", (void*)fset, feature_subset_name(fset->subset));
      show_backtrace(2);
      dbgrpt_vcp_feature_set(fset, 1);
   }

   DDCA_Feature_List vcplist = {{0}};
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   int ndx = 0;
   for (; ndx < fset->members->len; ndx++) {
      VCP_Feature_Table_Entry * vcp_entry = g_ptr_array_index(fset->members,ndx);
      feature_list_add(&vcplist, vcp_entry->code);
#ifdef OLD
      // DBGMSG("Setting feature: 0x%02x", vcp_entry->code);
      int flagndx   = vcp_code >> 3;
      int shiftct   = vcp_code & 0x07;
      Byte flagbit  = 0x01 << shiftct;
      // printf("(%s) vcp_code=0x%02x, flagndx=%d, shiftct=%d, flagbit=0x%02x\n",
      //        __func__, vcp_code, flagndx, shiftct, flagbit);
      vcplist.bytes[flagndx] |= flagbit;
      // uint8_t bval = vcplist.bytes[flagndx];
      // printf("(%s) vcplist.bytes[%d] = 0x%02x\n",  __func__, flagndx, bval);
#endif
   }

   if (debug || IS_TRACING()) {
      DBGMSG("Returning: %s", feature_list_string(&vcplist, "", " "));
      // rpt_hex_dump(vcplist.bytes, 32, 1);
   }

   return vcplist;
}
#endif


void init_vcp_feature_set() {

}

