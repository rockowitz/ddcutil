/** @file app_dynamic_features.c */

// Copyright (C) 2018-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "ddcutil_types.h"
#include "ddcutil_status_codes.h"

#include "util/error_info.h"
#include "util/string_util.h"
/** \endcond */

#include "base/core.h"
#include "base/dynamic_features.h"
#include "base/monitor_model_key.h"

#include "dynvcp/dyn_feature_files.h"

#include "app_dynamic_features.h"


// extern bool enable_dynamic_features;   // *** TEMP ***


/** Wraps call to #dfr_check_by_dref(), writing error messages
 *  for errors reported.
 *
 *  \param dref  display reference
 */
void check_dynamic_features(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_UDF, "enable_dynamic_features=%s", sbool(enable_dynamic_features));

   if (!enable_dynamic_features)    // global variable
      goto bye;

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

bye:
   DBGTRC_DONE(debug, DDCA_TRC_UDF, "");
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


#include <app_ddcutil/app_dynamic_features.h>

