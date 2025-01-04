/** @file ddc_phantom_displays.h  Phantom display detection */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_PHANTOM_DISPLAYS_H_
#define DDC_PHANTOM_DISPLAYS_H_

#include <stdbool.h>
#include <glib-2.0/glib.h>

extern bool  detect_phantom_displays;

bool filter_phantom_displays(GPtrArray * all_displays);
void init_ddc_phantom_displays();

#endif /* DDC_PHANTOM_DISPLAYS_H_ */
