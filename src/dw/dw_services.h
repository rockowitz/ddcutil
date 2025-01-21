/** @file dw_services.h
 *
 * dw layer initialization and configuration, statistics management
 */

// Copyright (C) 2024-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_SERVICES_H_
#define DW_SERVICES_H_

#include <stdbool.h>
#include <stdio.h>

#include "public/ddcutil_types.h"

void init_dw_services();
void terminate_dw_services();

#endif /* DW_SERVICES_H_ */
