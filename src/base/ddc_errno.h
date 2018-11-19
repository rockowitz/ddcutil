/** \file
 * Error codes internal to **ddcutil**.
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_ERRNO_H_
#define DDC_ERRNO_H_

#include "public/ddcutil_status_codes.h"

#include "base/status_code_mgt.h"


Status_Code_Info * ddcrc_find_status_code_info(int rc);

bool ddc_error_name_to_number(const char * errno_name, Status_DDC * perrno);

// Returns status code description:
char * ddcrc_desc_t(int rc);

bool ddcrc_is_derived_status_code(Public_Status_Code gsc);

bool ddcrc_is_not_error(Public_Status_Code gsc);

#endif /* DDC_ERRNO_H_ */
