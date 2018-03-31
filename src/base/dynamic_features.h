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

#include "util/error_info.h"

typedef struct {
   char *          mfg_id;     // [EDID_MFG_ID_FIELD_SIZE];
   char *          model_name;  // [EDID_MODEL_NAME_FIELD_SIZE];
   uint16_t        product_code;
   GHashTable *    features;     // array of DDCA_Feature_Metadata
} Dynamic_Features_Rec;


Error_Info *
create_monitor_dynamic_features(
      const char *            mfg_id,
      const char *            model_name,
      uint16_t                product_code,
      GPtrArray *             lines,
      const char *            filename,     // may be NULL
      Dynamic_Features_Rec ** dynamic_featues_loc);

#endif /* BASE_DYNAMIC_FEATURES_H_ */
