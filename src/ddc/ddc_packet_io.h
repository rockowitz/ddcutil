/** @file ddc_packet_io.h
 *
 *  Functions for performing DDC packet IO,  Handles I2C bus retry.
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_PACKET_IO_H_
#define DDC_PACKET_IO_H_

#include <stdbool.h>

#include "util/error_info.h"

#include "base/core.h"
#include "base/ddc_packets.h"
#include "base/displays.h"


extern bool DDC_Read_Bytewise;
extern bool simulate_null_msg_means_unsupported;

typedef enum {
   Write_Read_Flags_None = 0,
   Write_Read_Flag_All_Zero_Response_Ok = 1,
   Write_Read_Flag_Capabilities = 2,
   Write_Read_Flag_Table_Read = 4
} DDC_Write_Read_Flags ;

Error_Info * ddc_open_display(
      Display_Ref *    dref,
      Call_Options     callopts,
      Display_Handle** dh_loc);

Error_Info * ddc_close_display(
      Display_Handle * dh);

void ddc_close_display_wo_return(
      Display_Handle * dh);

void ddc_close_all_displays();

DDCA_Status ddc_validate_display_handle(Display_Handle * dh);

void ddc_dbgrpt_valid_display_handles(int depth);

Error_Info * ddc_write_only(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr);

Error_Info * ddc_write_only_with_retry(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr);

Error_Info * ddc_write_read(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
      bool             read_bytewise,
      int              max_read_bytes,
      Byte             expected_response_type,
      Byte             expected_subtype,
      DDC_Packet **    response_packet_ptr_loc
     );

Error_Info * ddc_write_read_with_retry(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
      int              max_read_bytes,
      Byte             expected_response_type,
      Byte             expected_subtype,
      DDC_Write_Read_Flags flags,
      DDC_Packet **    response_packet_ptr_loc
     );

void init_ddc_packet_io();

void terminate_ddc_packet_io();

#endif /* DDC_PACKET_IO_H_ */

