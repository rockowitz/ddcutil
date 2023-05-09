/** @file ddc_serialize.h */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_SERIALIZE_H_
#define DDC_SERIALIZE_H_

#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "base/displays.h"
#include "base/i2c_bus_base.h"

extern bool       display_caching_enabled;
extern GPtrArray* deserialized_displays;
extern GPtrArray* deserialized_buses;

void          ddc_enable_displays_cache(bool onoff);

Display_Ref * ddc_find_deserialized_display(int busno, Byte* edidbytes);

char *        ddc_serialize_displays_and_buses();
GPtrArray *   ddc_deserialize_displays(const char * jstring);
GPtrArray *   ddc_deserialize_buses(const char * jstring);

bool          ddc_store_displays_cache();
void          ddc_restore_displays_cache();
void          ddc_erase_displays_cache();

void          init_ddc_serialize();

#endif /* DDC_SERIALIZE_H_ */
