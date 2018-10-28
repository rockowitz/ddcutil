// dynamic_features.h

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \file
 * Dynamic Feature Record definition, creation, destruction, and conversion
 */

#ifndef BASE_DYNAMIC_FEATURES_H_
#define BASE_DYNAMIC_FEATURES_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <inttypes.h>
/** \endcond */

#include "ddcutil_types.h"

// #include "util/edid.h"
#include "util/error_info.h"

// #include "monitor_model_key.h"

typedef enum {
   DFR_FLAGS_NONE      = 0,
   DFR_FLAGS_NOT_FOUND = 1
} DFR_Flags;


#define DYNAMIC_FEATURES_REC_MARKER "DFRC"
typedef struct {
   char                       marker[4];
   char *                     mfg_id;       // [EDID_MFG_ID_FIELD_SIZE];
   char *                     model_name;   // [EDID_MODEL_NAME_FIELD_SIZE];
   uint16_t                   product_code;
   char *                     filename;     // source filename, if applicable
   DDCA_MCCS_Version_Spec     vspec;
   DFR_Flags                  flags;
   GHashTable *               features;     // hash table of DDCA_Feature_Metadata
} Dynamic_Features_Rec;


// value valid until next call:
char *
dfr_repr_t(
      Dynamic_Features_Rec * dfr);

Dynamic_Features_Rec *
dfr_new(
      const char *            mfg_id,
      const char *            model_name,
      uint16_t                product_code,
      const char *            filename);

void
dfr_free(
      Dynamic_Features_Rec *  frec);



Error_Info *
create_monitor_dynamic_features(
      const char *            mfg_id,
      const char *            model_name,
      uint16_t                product_code,
      GPtrArray *             lines,
      const char *            filename,     // may be NULL
      Dynamic_Features_Rec ** dynamic_features_loc);


DDCA_Feature_Metadata *
get_dynamic_feature_metadata(
      Dynamic_Features_Rec *  dfr,
      uint8_t                 feature_code);


// belongs in more generic location
char * interpret_ddca_feature_flags(DDCA_Version_Feature_Flags flags);
void
dbgrpt_feature_metadata(
      DDCA_Feature_Metadata * md,
      int                     depth);

void dbgrpt_dynamic_features_rec(
      Dynamic_Features_Rec*   dfr,
      int                     depth);


#endif /* BASE_DYNAMIC_FEATURES_H_ */
