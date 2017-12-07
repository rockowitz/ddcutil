/* ddc_multi_part_io.h
 *
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

/** \file
 * Capabilities read and Table feature read/write that require multiple
 * reads and writes for completion.
 */

#ifndef DDC_MULTI_PART_IO_H_
#define DDC_MULTI_PART_IO_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

// #include "util/coredefs.h"
#include "util/data_structures.h"

#include "base/ddc_error.h"
#include "base/displays.h"
#include "base/status_code_mgt.h"


// Statistics
void ddc_reset_multi_part_read_stats();
void ddc_report_multi_part_read_stats(int depth);

// Retry management
void ddc_set_max_multi_part_read_tries(int ct);
int  ddc_get_max_multi_part_read_tries();


Ddc_Error *
multi_part_read_with_retry(
   Display_Handle * dh,
   Byte             request_type,
   Byte             request_subtype,   // VCP feature code for table read, ignore for capabilities
   bool             all_zero_response_ok,
   Buffer**         ppbuffer);

Ddc_Error *
multi_part_write_with_retry(
     Display_Handle * dh,
     Byte             vcp_code,
     Buffer *         value_to_set);
#endif /* DDC_MULTI_PART_IO_H_ */
