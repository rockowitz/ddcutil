/** @file monitor_model_key.h
 *  Uniquely identifies a monitor model using its manufacturer id,
 *  model name, and product code, as listed in the EDID.
 */

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef MONITOR_MODEL_KEY_H_
#define MONITOR_MODEL_KEY_H_

#include <inttypes.h>

#include "public/ddcutil_types.h"

#include "util/edid.h"

// #define MONITOR_MODEL_KEY_MARKER "MMID"
/** Identifies a monitor model */
typedef struct {
// char                marker[4];
   char                mfg_id[DDCA_EDID_MFG_ID_FIELD_SIZE];
   char                model_name[DDCA_EDID_MODEL_NAME_FIELD_SIZE];
   uint16_t            product_code;
   bool                defined;
} DDCA_Monitor_Model_Key;


DDCA_Monitor_Model_Key
monitor_model_key_value(
      const char *   mfg_id,
      const char *   model_name,
      uint16_t       product_code);

DDCA_Monitor_Model_Key
monitor_model_key_undefined_value();

DDCA_Monitor_Model_Key
monitor_model_key_value_from_edid(Parsed_Edid * edid);

DDCA_Monitor_Model_Key *
monitor_model_key_new(
      const char *   mfg_id,
      const char *   model_name,
      uint16_t       product_code);

DDCA_Monitor_Model_Key *
monitor_model_key_new_from_edid(
      Parsed_Edid * edid);

#ifdef UNUSED
DDCA_Monitor_Model_Key *
monitor_model_key_undefined_new();
#endif

void
monitor_model_key_free(
      DDCA_Monitor_Model_Key * model_id);

char *
model_id_string(
      const char *  mfg,
      const char *  model_name,
      uint16_t      product_code);

// needed at API level?
DDCA_Monitor_Model_Key
monitor_model_key_assign(DDCA_Monitor_Model_Key old);

bool
monitor_model_key_eq(
      DDCA_Monitor_Model_Key mmk1,
      DDCA_Monitor_Model_Key mmk2);

#ifdef UNUSED
bool monitor_model_key_is_defined(DDCA_Monitor_Model_Key mmk);
#endif

char *
monitor_model_string(
      DDCA_Monitor_Model_Key * model_id);

char * mmk_repr(DDCA_Monitor_Model_Key mmk);

#endif /* MONITOR_MODEL_KEY_H_ */
