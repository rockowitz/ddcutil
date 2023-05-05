/** @file ddc_serialize.h */

// Copyright (C) 2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_SERIALIZE_H_
#define DDC_SERIALIZE_H_

#include <glib-2.0/glib.h>

#include "base/i2c_bus_base.h"

char * serialize_bus_info(I2C_Bus_Info * info);

char * ddc_serialize_displays();
GPtrArray * ddc_deserialize_displays(const char * jstring);

#endif /* DDC_SERIALIZE_H_ */
