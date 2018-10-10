// i2c_bus_selector.h

// Generalized bus_info finder, now used only within usb_edid.c to find
// a fallback EDID

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef I2C_BUS_SELECTOR_H_
#define I2C_BUS_SELECTOR_H_

#include "util/coredefs.h"

#include "i2c_bus_core.h"

// Complex Bus_Info retrieval
I2C_Bus_Info * i2c_find_bus_info_by_mfg_model_sn(
              const char * mfg_id,
              const char * model,
              const char * sn,
              Byte         findopts);


#endif /* I2C_BUS_SELECTOR_H_ */
