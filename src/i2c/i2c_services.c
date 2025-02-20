/** @file i2c_services.c  */

// Copyright (C) 2022-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "base/i2c_bus_base.h"

#include "i2c_bus_core.h"
#include "i2c_edid.h"
#include "i2c_execute.h"
#include "i2c_strategy_dispatcher.h"

/** Master initializer for directory i2c */
void init_i2c_services() {
   init_i2c_bus_core();
   init_i2c_edid();
   init_i2c_execute();
   init_i2c_strategy_dispatcher();
}

void terminate_i2c_services() {
   terminate_i2c_bus_base();
}
