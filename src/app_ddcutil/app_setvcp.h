/** @file app_setvcp.h
 *
 *  Implement the SETVCP command
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APP_SETVCP_H_
#define APP_SETVCP_H_

#include "util/error_info.h"

#include "cmdline/parsed_cmd.h"
#include "base/displays.h"
#include "base/status_code_mgt.h"

Error_Info *
app_set_vcp_value(
        Display_Handle * dh,
#ifdef OLD
        char *           feature,
#else
      Byte             feature_code,
      Setvcp_Value_Type value_type,
#endif
        char *           new_value,
        bool             force);

#endif /* APP_SETVCP_H_ */
