/** \file ddc_multi_part_io.h
 *
 * Capabilities read and Table feature read/write that require multiple
 * reads and writes for completion.
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "ddc/ddc_packet_io.h"

//temp:
extern int multi_part_null_adjustment_millis;

Error_Info *
multi_part_read_with_retry(
   Display_Handle * dh,
   Byte             request_type,
   Byte             request_subtype,   // VCP feature code for table read, ignore for capabilities
   DDC_Write_Read_Flags write_read_flags,
   Buffer**         ppbuffer);

Error_Info *
multi_part_write_with_retry(
     Display_Handle * dh,
     Byte             vcp_code,
     Buffer *         value_to_set);

void
init_ddc_multi_part_io();

#endif /* DDC_MULTI_PART_IO_H_ */
