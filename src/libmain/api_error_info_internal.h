// api_error_info_internal.h

// Copyright (C) 2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef API_ERROR_INFO_INTERNAL_H_
#define API_ERROR_INFO_INTERNAL_H_

#include "ddcutil_types.h"

#include "util/error_info.h"

DDCA_Error_Detail * error_info_to_ddca_detail(Error_Info * erec);
DDCA_Error_Detail * dup_error_detail(DDCA_Error_Detail * old);
void free_error_detail(DDCA_Error_Detail * ddca_erec);
void report_error_detail(DDCA_Error_Detail * ddca_erec, int depth);

void free_thread_error_detail();
DDCA_Error_Detail * get_thread_error_detail();
void save_thread_error_detail(DDCA_Error_Detail * error_detail);

#endif /* API_ERROR_INFO_INTERNAL_H_ */
