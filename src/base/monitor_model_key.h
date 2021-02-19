/** \file monitor_model_key.h
 *  Uniquely identifies a monitor model using its manufacturer id,
 *  model name, and product code, as listed in the EDID.
 */

// Copyright (C) 2018-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef MONITOR_MODEL_KEY_H_
#define MONITOR_MODEL_KEY_H_

#include <inttypes.h>

#include "private/ddcutil_types_private.h"

DDCA_Monitor_Model_Key
monitor_model_key_value(
      const char *   mfg_id,
      const char *   model_name,
      uint16_t       product_code);

DDCA_Monitor_Model_Key
monitor_model_key_undefined_value();

DDCA_Monitor_Model_Key *
monitor_model_key_new(
      const char *   mfg_id,
      const char *   model_name,
      uint16_t       product_code);

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
