/** @file app_dynamic_features.c */

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
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
#include "base/rtti.h"

#include "dynvcp/dyn_feature_files.h"

#include "app_dynamic_features.h"


/** Wraps call to #dfr_check_by_dref(), writing error messages
 *  for errors reported.
 *
 *  \param dref  display reference
 */
void app_check_dynamic_features(Display_Ref * dref) {
   bool debug = false;
   DBGTRC_STARTING(debug, DDCA_TRC_TOP|DDCA_TRC_UDF, "dref=%s, enable_dynamic_features=%s",
                          dref_repr_t(dref), sbool(enable_dynamic_features));

   if (!enable_dynamic_features)    // global variable
      goto bye;

   Error_Info * errs = dfr_check_by_dref(dref);
   DDCA_Output_Level ol = get_output_level();
   if (errs) {
      if (errs->status_code == DDCRC_NOT_FOUND) {
         if (ol >= DDCA_OL_VERBOSE) {
            f0printf(fout(), "%s\n", errs->detail);
         }
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
      if (ol >= DDCA_OL_VERBOSE) {
         f0printf(fout(), "Processed feature definition file: %s\n",
                          dref->dfr->filename);
      }
   }

bye:
   DBGTRC_DONE(debug, DDCA_TRC_UDF, "");
}


void init_app_dynamic_features() {
   RTTI_ADD_FUNC(app_check_dynamic_features);
}
