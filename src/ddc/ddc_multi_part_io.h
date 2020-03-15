/** \file ddc_multi_part_io.h
 *
 * Capabilities read and Table feature read/write that require multiple
 * reads and writes for completion.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_MULTI_PART_IO_H_
#define DDC_MULTI_PART_IO_H_

/** \cond */
#include <stdbool.h>

#include "util/error_info.h"
#include "util/data_structures.h"
/** \endcond */

#include "base/displays.h"
#include "base/status_code_mgt.h"


// Statistics
#ifdef OLD
void ddc_reset_multi_part_read_stats();
void ddc_reset_multi_part_write_stats();
#endif
void ddc_report_multi_part_read_stats(int depth);
void ddc_report_multi_part_write_stats(int depth);

// Retry management
// void ddc_set_max_multi_part_read_tries(int ct);
Retry_Op_Value ddc_get_max_multi_part_read_tries();

// void ddc_set_max_multi_part_write_tries(int ct);
Retry_Op_Value  ddc_get_max_multi_part_write_tries();


Error_Info *
multi_part_read_with_retry(
   Display_Handle * dh,
   Byte             request_type,
   Byte             request_subtype,   // VCP feature code for table read, ignore for capabilities
   bool             all_zero_response_ok,
   Buffer**         ppbuffer);

Error_Info *
multi_part_write_with_retry(
     Display_Handle * dh,
     Byte             vcp_code,
     Buffer *         value_to_set);

void
init_ddc_multi_part_io();

#endif /* DDC_MULTI_PART_IO_H_ */
