/* ddc_packet_io.h
 *
 * Created on: Jun 13, 2014
 *     Author: rock
 *
 *  Functions for performing DDC packet IO, using either the I2C bus API
 *  or the ADL API, as appropriate.
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef DDC_PACKET_IO_H_
#define DDC_PACKET_IO_H_

#include <stdbool.h>

#include "base/common.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/util.h"


Display_Handle* ddc_open_display(Display_Ref * dref,  Failure_Action failure_action);
void            ddc_close_display(Display_Handle * dh);


// Retry management
void ddc_set_max_write_only_exchange_tries(int ct);
int  ddc_get_max_write_only_exchange_tries();
void ddc_set_max_write_read_exchange_tries(int ct);
int  ddc_get_max_write_read_exchange_tries();

// Retry statistics
void ddc_reset_write_only_stats();
void ddc_report_write_only_stats();
void ddc_reset_write_read_stats();
void ddc_report_write_read_stats();

Global_Status_Code ddc_write_only(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr);

Global_Status_Code ddc_write_only_with_retry(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr);

Global_Status_Code ddc_write_read(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
      int              max_read_bytes,
      Byte             expected_response_type,
      Byte             expected_subtype,
      DDC_Packet **    response_packet_ptr_loc
     );

Global_Status_Code ddc_write_read_with_retry(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
      int              max_read_bytes,
      Byte             expected_response_type,
      Byte             expected_subtype,
      bool             all_zero_response_ok,
      DDC_Packet **    response_packet_ptr_loc
     );

#endif /* DDC_PACKET_IO_H_ */
