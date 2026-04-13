/** \file utilrpt.c
 *
 * Report functions for other utility modules.
 *
 * Avoids complex dependencies between report_util.c and other util files.
 */

// Copyright (C) 2014-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "data_structures.h"
#include "report_util.h"

#include "utilrpt.h"

/** Displays all fields of a #Buffer.
 *  This is a debugging function.
 *
 *  @param buffer   pointer to Buffer instance
 *  @param depth    logical indentation depth
 *
 *  @remark
 *  Output is written to the current report destination.
 */
void dbgrpt_buffer(Buffer * buffer, int depth) {
   rpt_vstring(depth,
               "Buffer at %p,  bytes addr=%p, len=%d, max_size=%d",
                buffer, buffer->bytes, buffer->len, buffer->buffer_size);
   // printf("  bytes end addr=%p\n", buffer->bytes+buffer->buffer_size);
   if (buffer->bytes)
      rpt_hex_dump(buffer->bytes, buffer->len, depth);
}
