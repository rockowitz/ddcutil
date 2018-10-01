/* app_dynamic_features.c
 *
 * <copyright>
 * Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
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




// #include <ctype.h>

#include <assert.h>
#include <stddef.h>
// #include <glib-2.0/glib.h>
#include <string.h>
// #include <wordexp.h>
// #include <unistd.h>

#include "ddcutil_types.h"
#include "ddcutil_status_codes.h"

#include "util/error_info.h"
// #include "util/edid.h"
// #include "util/glib_util.h"
#include "util/string_util.h"
// #include "util/file_util.h"

#include "base/core.h"
// #include "base/displays.h"
#include "base/dynamic_features.h"
#include "base/monitor_model_key.h"

#include "ddc/ddc_dynamic_features.h"

#include "app_dynamic_features.h"


extern bool enable_dynamic_features;   // *** TEMP ***


/** Wraps call to #dfr_check_by_dref(), writing error messages
 *  for errors reported.
 *
 *  \param dref  display reference
 */
void check_dynamic_features(Display_Ref * dref) {
   if (!enable_dynamic_features)    // global variable
      return;

   bool debug = false;
   DBGMSF(debug, "Starting. ");

   // bool wrote_output = false;
   Error_Info * errs = dfr_check_by_dref(dref);
   DDCA_Output_Level ol = get_output_level();
   if (errs) {
      if (errs->status_code == DDCRC_NOT_FOUND) {
         if (ol >= DDCA_OL_VERBOSE) {
            f0printf(fout(), "%s\n", errs->detail);
            // wrote_output = true;
         }
      }
      else {
         // errinfo_report(errs, 1);
         f0printf(fout(), "%s\n", errs->detail);
         for (int ndx = 0; ndx < errs->cause_ct; ndx++) {
            f0printf(fout(), "   %s\n", errs->causes[ndx]->detail);
         }
         // wrote_output = true;
      }
      errinfo_free(errs);
   }
   else {
      // dbgrpt_dynamic_features_rec(dfr, 1);
      if (ol >= DDCA_OL_VERBOSE) {
         f0printf(fout(), "Processed feature definition file: %s\n",
                          dref->dfr->filename);
         // wrote_output = true;
      }
   }

   DBGMSF(debug, "Done.");
}


#ifdef OLD
void check_dynamic_features_old(Display_Ref * dref) {
   if (!enable_dynamic_features)    // global variable
      return;

   bool debug = false;
   DBGMSF(debug, "Starting. ");
   if ( !(dref->flags & DREF_DYNAMIC_FEATURES_CHECKED) ) {
      // DBGMSF(debug, "DREF_DYNAMIC_FEATURES_CHECKED not yet set");
      dref->dfr = NULL;
      DDCA_Output_Level ol = get_output_level();

      Dynamic_Features_Rec * dfr = NULL;
      Error_Info * errs = dfr_load_by_edid(dref->pedid, &dfr);
      if (errs) {
         if (errs->status_code == DDCRC_NOT_FOUND) {
            if (ol >= DDCA_OL_VERBOSE)
               f0printf(fout(), "%s\n", errs->detail);
         }
         else {
            // errinfo_report(errs, 1);
            f0printf(fout(), "%s\n", errs->detail);
            for (int ndx = 0; ndx < errs->cause_ct; ndx++) {
               f0printf(fout(), "   %s\n", errs->causes[ndx]->detail);
            }
         }
         errinfo_free(errs);
      }
      else {
         // dbgrpt_dynamic_features_rec(dfr, 1);
         if (ol >= DDCA_OL_VERBOSE)
            f0printf(fout(), "Processed feature definition file: %s\n", dfr->filename);
         dref->dfr = dfr;
      }

      dref->flags |= DREF_DYNAMIC_FEATURES_CHECKED;
   }
   DBGMSF(debug, "Done.");
}
#endif


#include "app_dynamic_features.h"

