/** @file ddc_serialize.h */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_SERIALIZE_H_
#define DDC_SERIALIZE_H_

#include <glib-2.0/glib.h>
#include <stdbool.h>

#include "base/displays.h"

extern bool   display_caching_enabled;
void          ddc_enable_displays_cache(bool onoff);
char *        ddc_displays_cache_file_name();
bool          ddc_store_displays_cache();
void          ddc_restore_displays_cache();
void          ddc_erase_displays_cache();
Display_Ref * ddc_find_deserialized_display(int busno, Byte* edidbytes);
void          init_ddc_serialize();
void          terminate_ddc_serialize();

#endif /* DDC_SERIALIZE_H_ */
