/** @file ddc_serialize.h */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_SERIALIZE_H_
#define DDC_SERIALIZE_H_

#include <glib-2.0/glib.h>
#include <jansson.h>
#include <stdbool.h>

#include "util/edid.h"

#include "base/displays.h"
#include "base/monitor_model_key.h"

extern bool   display_caching_enabled;
void          ddc_enable_displays_cache(bool onoff);
char *        ddc_displays_cache_file_name();
bool          ddc_store_displays_cache();
void          ddc_restore_displays_cache();
void          ddc_erase_displays_cache();
Display_Ref * ddc_find_deserialized_display(int busno, Byte* edidbytes);
void          init_ddc_serialize();
void          terminate_ddc_serialize();

// Display_Refs restored from the cache file by ddc_restore_displays_cache();
// searched by ddc_find_deserialized_display().
extern GPtrArray* deserialized_displays;

// The functions below implement the JSON (de)serialization of a single
// Display_Ref and its component fields. They are not otherwise used
// outside ddc_serialize.c; exposed here for unit testing.
json_t*                serialize_dpath(DDCA_IO_Path iopath);
DDCA_IO_Path           deserialize_dpath(json_t* jpath);
json_t *               serialize_vspec(DDCA_MCCS_Version_Spec vspec);
DDCA_MCCS_Version_Spec deserialize_vspec(json_t* jpath);
json_t *               serialize_parsed_edid(Parsed_Edid * pedid);
Parsed_Edid *          deserialize_parsed_edid(json_t* jpath);
json_t *               serialize_mmk(Monitor_Model_Key * mmk);
Monitor_Model_Key *    deserialize_mmid(json_t* jpath);
json_t*                serialize_one_display(Display_Ref * dref);
Display_Ref *          deserialize_one_display(json_t* disp_node);

#endif /* DDC_SERIALIZE_H_ */
