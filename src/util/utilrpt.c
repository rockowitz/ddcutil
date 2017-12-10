/* utilrpt.c
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

/** \f
* Report functions for other utility modules.
* Avoids complex dependencies between report_util.c and other
* util files.
*/

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
