/* monitor_model_key.h
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

#ifndef MONITOR_MODEL_KEY_H_
#define MONITOR_MODEL_KEY_H_


// #include <inttypes.h>
// #include <stdlib.h>

#include "ddcutil_types.h"

// #include "util/edid.h"


DDCA_Monitor_Model_Key
monitor_model_key_value(
      char *   mfg_id,
      char *   model_name,
      uint16_t product_code);

DDCA_Monitor_Model_Key
monitor_model_key_undefined_value();

DDCA_Monitor_Model_Key *
monitor_model_key_new(
      char *   mfg_id,
      char *   model_name,
      uint16_t product_code);

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


#endif /* MONITOR_MODEL_KEY_H_ */
