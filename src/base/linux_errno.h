/** \file linux_errno.h
 * Linux errno descriptions
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef BASE_LINUX_ERRNO_H_
#define BASE_LINUX_ERRNO_H_

#include "base/status_code_mgt.h"

void init_linux_errno();

// Interpret system error number
// not thread safe, always points to same internal buffer, contents overwritten on each call
char * linux_errno_name(int error_number);
char * linux_errno_desc(int error_number);

Status_Code_Info * get_errno_info(int errnum);
Status_Code_Info * get_negative_errno_info(int errnum);

bool errno_name_to_number(const char * errno_name, int * perrno);
bool errno_name_to_modulated_number(const char * errno_name, Public_Status_Code * p_error_number);

#endif /* BASE_LINUX_ERRNO_H_ */
