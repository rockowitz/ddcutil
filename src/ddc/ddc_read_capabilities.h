/** @file ddc_read_capabilities.h
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_READ_CAPABILITIES_H_
#define DDC_READ_CAPABILITIES_H_

/** \cond */
#include "util/error_info.h"
/** \endcond */

#include "base/displays.h"
#include "base/status_code_mgt.h"


// Get capability string for monitor.

Error_Info *
get_capabilities_string(
      Display_Handle * dh,
      char**           pcaps);

#endif /* DDC_READ_CAPABILITIES_H_ */
