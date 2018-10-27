/** \file ddc_packet_io.h
 *
 *  Functions for performing DDC packet IO, using either the I2C bus API
 *  or the ADL API, as appropriate.  Handles I2C bus retry.
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_PACKET_IO_H_
#define DDC_PACKET_IO_H_

#include <stdbool.h>

#include "util/error_info.h"

#include "base/core.h"
#include "base/ddc_packets.h"
#include "base/displays.h"


// bool all_zero(Byte * bytes, int bytec);

Public_Status_Code ddc_open_display(
      Display_Ref *    dref,
      Call_Options     callopts,
      Display_Handle** pdh);
void ddc_close_display(Display_Handle * dh);


// Retry management
void ddc_set_max_write_only_exchange_tries(int ct);
int  ddc_get_max_write_only_exchange_tries();
void ddc_set_max_write_read_exchange_tries(int ct);
int  ddc_get_max_write_read_exchange_tries();

// Retry statistics
void ddc_reset_write_only_stats();
void ddc_report_write_only_stats(int depth);
void ddc_reset_write_read_stats();
void ddc_report_write_read_stats(int depth);

Error_Info * ddc_write_only(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr);

Error_Info * ddc_write_only_with_retry(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr);

Error_Info * ddc_write_read(
      Display_Handle * dh,
      DDC_Packet *     request_packet_ptr,
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
      bool             all_zero_response_ok,
  //  bool             retry_null_response,
      DDC_Packet **    response_packet_ptr_loc
     );

#endif /* DDC_PACKET_IO_H_ */
