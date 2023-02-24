/** @file api_services_internal.c */

// Copyright (C) 2021-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>

#include "base/core.h"

#include "api_capabilities_internal.h"
#include "api_displays_internal.h"
#include "api_feature_access_internal.h"
#include "api_metadata_internal.h"

#include "api_services_internal.h"


void init_api_services() {
   init_api_capabilities();
   init_api_displays();
   init_api_feature_access();
   init_api_metadata();
}
