/* dynamic_features.h
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

#ifndef BASE_DYNAMIC_FEATURES_H_
#define BASE_DYNAMIC_FEATURES_H_

#include <glib-2.0/glib.h>
#include <inttypes.h>
// #include <stdlib.h>

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
   GHashTable *               features;     // array of DDCA_Feature_Metadata
} Dynamic_Features_Rec;


Dynamic_Features_Rec *
dfr_new(
      const char *            mfg_id,
      const char *            model_name,
      uint16_t                product_code,
      const char *            filename);

void
dfr_free(
      Dynamic_Features_Rec *  frec);

void
dbgrpt_dfr(
      Dynamic_Features_Rec *  dfr,
      int                     depth);

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


#endif /* BASE_DYNAMIC_FEATURES_H_ */
