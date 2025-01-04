/** @file ddc_phantom_displays.c  Phantom display detection*/

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later
 
#include "config.h"

#include <assert.h>
#include <glib-2.0/glib.h>

#include "util/sysfs_util.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/i2c_bus_base.h"
#include "base/rtti.h"

#include "i2c/i2c_sysfs_base.h"

#include "ddc_phantom_displays.h"

 // Default trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_DDC;

bool detect_phantom_displays = true;


STATIC bool
edid_ids_match(Parsed_Edid * edid1, Parsed_Edid * edid2) {
   bool result = false;
   result = streq(edid1->mfg_id,        edid2->mfg_id)        &&
            streq(edid1->model_name,    edid2->model_name)    &&
            edid1->product_code      == edid2->product_code   &&
            streq(edid1->serial_ascii,  edid2->serial_ascii)  &&
            edid1->serial_binary     == edid2->serial_binary;
   return result;
}


/** Check if an invalid #Display_Reference can be regarded as a phantom
 *  of a given valid #Display_Reference.
 *
 *  @param  invalid_dref
 *  @param  valid_dref
 *  @return true/false
 *
 *  - Both are /dev/i2c devices
 *  - The EDID id fields must match
 *  - For the invalid #Display_Reference:
 *    - attribute status must exist and equal "disconnected"
 *    - attribute enabled must exist and equal "disabled"
 *    - attribute edid must not exist
 */
STATIC bool
is_phantom_display(Display_Ref* invalid_dref, Display_Ref * valid_dref) {
   bool debug = false;
   char * invalid_repr = g_strdup(dref_repr_t(invalid_dref));
   char *   valid_repr = g_strdup(dref_repr_t(valid_dref));
   DBGTRC_STARTING(debug, TRACE_GROUP, "invalid_dref=%s, valid_dref=%s",
                 invalid_repr, valid_repr);
   free(invalid_repr);
   free(valid_repr);

   bool result = false;
   // User report has shown that 128 byte EDIDs can differ for the valid and
   // invalid display.  Specifically, byte 24 was seen to differ, with one
   // having RGB 4:4:4 and the other RGB 4:4:4 + YCrCb 4:2:2!.  So instead of
   // simply byte comparing the 2 EDIDs, check the identifiers.
   if (edid_ids_match(invalid_dref->pedid, valid_dref->pedid)) {
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "EDIDs match");
      if (invalid_dref->io_path.io_mode == DDCA_IO_I2C &&
            valid_dref->io_path.io_mode == DDCA_IO_I2C)
      {
         int invalid_busno = invalid_dref->io_path.path.i2c_busno;
         // int valid_busno = valid_dref->io_path.path.i2c_busno;
         char buf0[40];
         snprintf(buf0, 40, "/sys/bus/i2c/devices/i2c-%d", invalid_busno);
         bool old_silent = set_rpt_sysfs_attr_silent(!(debug|| IS_TRACING()));
         char * invalid_rpath = NULL;
         bool ok = RPT_ATTR_REALPATH(0, &invalid_rpath, buf0, "device");
         if (ok) {
            result = true;
            char * attr_value = NULL;
            possibly_write_detect_to_status_by_connector_path(invalid_rpath);
            ok = RPT_ATTR_TEXT(0, &attr_value, invalid_rpath, "status");
            if (!ok  || !streq(attr_value, "disconnected"))
               result = false;
            ok = RPT_ATTR_TEXT(0, &attr_value, invalid_rpath, "enabled");
            if (!ok  || !streq(attr_value, "disabled"))
               result = false;
            GByteArray * edid;
            ok = RPT_ATTR_EDID(0, &edid, invalid_rpath, "edid");    // is "edid" needed
            if (ok) {
               result = false;
               g_byte_array_free(edid, true);
            }
         }
         set_rpt_sysfs_attr_silent(old_silent);
      }
   }
   DBGTRC_DONE(debug, TRACE_GROUP,    "Returning: %s", sbool(result) );
   return result;
}


/** Tests if 2 #Display_Ref instances have each have EDIDs and
 *  they are identical.
 *  @param dref1
 *  @param dref2
 *  @return true/false
 */
bool drefs_edid_equal(Display_Ref * dref1, Display_Ref * dref2) {
   bool debug = false;
   if (IS_DBGTRC(debug, DDCA_TRC_NONE)) {
      char * s = g_strdup( dref_repr_t(dref2));
      DBGTRC_STARTING(debug, DDCA_TRC_NONE, "dref1=%s, dref2=%s", dref_repr_t(dref1), s);
      free(s);
   }
   assert(dref1);
   assert(dref2);
   Parsed_Edid * pedid1 = dref1->pedid;
   Parsed_Edid * pedid2 = dref2->pedid;
   bool edids_equal = false;
   if (pedid1 && pedid2) {
      if (memcmp(pedid1->bytes, pedid2->bytes, 128) == 0) {
         edids_equal = true;
      }
   }
   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, edids_equal, "");
   return edids_equal;
}


/** Checks if any 2 #Display_Ref instances in a GPtrArray of instances
*   have identical EDIDs.
*   @param  drefs  array of Display_Refs
*   @return true/false
*/
static bool
has_duplicate_edids(GPtrArray * drefs) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "drefs->len = %d", drefs->len);
   bool found_duplicate = false;
   for (int i = 0; i < drefs->len; i++) {
      for (int j = i+1; j < drefs->len; j++) {
         if (drefs_edid_equal(g_ptr_array_index(drefs, i), g_ptr_array_index(drefs, j)) ) {
            found_duplicate = true;
            break;
         }
      }
   }
   DBGTRC_RET_BOOL(debug, DDCA_TRC_NONE, found_duplicate, "");
   return found_duplicate;
}


/** Mark phantom displays.
 *
 *  Split the #Display_Ref's in a GPtrArray into those that have
 *  already been determined to be valid (dispno > 0) and those
 *  that are invalid (dispno < 0).
 *
 *  For each invalid display ref, check to see if it is a phantom display
 *  corresponding to one of the valid displays.  If so, set its dispno
 *  to DISPNO_INVALID and save a pointer to the valid display ref.
 *
 *  @param all_displays array of pointers to #Display_Ref
 *  @return true if phantom displays detected, false if not
 *
 *  @remark
 *  This handles the case where DDC communication works for one /dev/i2c bus
 *  but not another. It also handles the case where there are 2 valid display
 *  refs and the connector for one has name DPMST.
 */
bool
filter_phantom_displays(GPtrArray * all_displays) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "all_displays->len=%d, detect_phantom_displays=%s",
         all_displays->len, sbool(detect_phantom_displays));

   bool phantom_displays_found = false;
   if (detect_phantom_displays && all_displays->len > 1) {
      GPtrArray* valid_displays   = g_ptr_array_sized_new(all_displays->len);
      GPtrArray* invalid_displays = g_ptr_array_sized_new(all_displays->len);
      GPtrArray* valid_non_mst_displays = g_ptr_array_sized_new(all_displays->len);
      GPtrArray* valid_mst_displays     = g_ptr_array_sized_new(all_displays->len);
      for (int ndx = 0; ndx < all_displays->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(all_displays, ndx);
         if (dref->io_path.io_mode == DDCA_IO_I2C) {
            TRACED_ASSERT( memcmp(dref->marker, DISPLAY_REF_MARKER, 4) == 0 );
            if (dref->dispno < 0)     // DISPNO_INVALID, DISPNO_PHANTOM, DISPNO_REMOVED
               g_ptr_array_add(invalid_displays, dref);
            else
               g_ptr_array_add(valid_displays, dref);
         }
      }

      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "%d valid displays, %d invalid displays",
                                 valid_displays->len, invalid_displays->len);
      if (invalid_displays->len > 0  && valid_displays->len > 0 ) {
         for (int invalid_ndx = 0; invalid_ndx < invalid_displays->len; invalid_ndx++) {
            Display_Ref * invalid_ref = g_ptr_array_index(invalid_displays, invalid_ndx);
            for (int valid_ndx = 0; valid_ndx < valid_displays->len; valid_ndx++) {
               Display_Ref *  valid_ref = g_ptr_array_index(valid_displays, valid_ndx);
               if (is_phantom_display(invalid_ref, valid_ref)) {
                  invalid_ref->dispno = DISPNO_PHANTOM;    // -2
                  invalid_ref->actual_display = valid_ref;
               }
            }
         }
      }

      for (int ndx = 0; ndx < valid_displays->len; ndx++) {
         Display_Ref * dref = g_ptr_array_index(valid_displays, ndx);
         I2C_Bus_Info * businfo = dref->detail;
         char * bus_name = get_i2c_device_sysfs_name(businfo->busno);
         if (streq(bus_name, "DPMST"))
            g_ptr_array_add(valid_mst_displays, dref);
         else
            g_ptr_array_add(valid_non_mst_displays, dref);
         free(bus_name);
      }

      if (valid_mst_displays->len > 0 && valid_non_mst_displays->len > 0) {
         // handle remote possibility of 2 monitors with identical edid:
         if (!has_duplicate_edids(valid_non_mst_displays)) {
            for (int mst_ndx = 0; mst_ndx < valid_mst_displays->len; mst_ndx++) {
               Display_Ref * valid_mst_display_ref = g_ptr_array_index(valid_mst_displays, mst_ndx);
               for (int non_mst_ndx = 0; non_mst_ndx < valid_non_mst_displays->len; non_mst_ndx++) {
                  Display_Ref * valid_non_mst_display_ref =
                        g_ptr_array_index(valid_non_mst_displays, non_mst_ndx);
                  Parsed_Edid * pedid1 = valid_mst_display_ref->pedid;
                  Parsed_Edid * pedid2 = valid_non_mst_display_ref->pedid;
                  if (pedid1 && pedid2) {
                     if (memcmp(pedid1->bytes, pedid2->bytes, 128) == 0) {
                        valid_non_mst_display_ref->dispno = DISPNO_PHANTOM;
                        valid_non_mst_display_ref->actual_display = valid_mst_display_ref;
                     }
                  }
               }
            }
         }
      }
      DBGTRC_NOPREFIX(debug, TRACE_GROUP, "%d valid mst_displays, %d valid_non_mst_displays",
                                    valid_mst_displays->len, valid_non_mst_displays->len);

      phantom_displays_found = invalid_displays->len > 0;
      // n. frees the underlying array, but not the Display_Refs pointed to by
      // array members, since no GDestroyNotify() function defined
      g_ptr_array_free(valid_mst_displays, true);
      g_ptr_array_free(valid_non_mst_displays, true);
      g_ptr_array_free(invalid_displays, true);
      g_ptr_array_free(valid_displays, true);
   }
   DBGTRC_RET_BOOL(debug, TRACE_GROUP, phantom_displays_found, "");
   return phantom_displays_found;
}


void init_ddc_phantom_displays() {
   RTTI_ADD_FUNC(drefs_edid_equal);
   RTTI_ADD_FUNC(filter_phantom_displays);
   RTTI_ADD_FUNC(has_duplicate_edids);
   RTTI_ADD_FUNC(is_phantom_display);
}
