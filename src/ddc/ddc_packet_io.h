/*  ddc_packet_io.h
 *
 *  Created on: Jun 13, 2014
 *      Author: rock
 *
 *  Functions for performing DDC packet IO, using either the I2C bus API
 *  or the ADL API, as appropriate.
 */

#ifndef DDC_PACKET_IO_H_
#define DDC_PACKET_IO_H_

#include <stdbool.h>

#include "base/common.h"
#include "base/ddc_packets.h"
#include "base/displays.h"
#include "base/util.h"


bool is_ddc_null_message(Byte * packet);

Display_Handle* ddc_open_display(Display_Ref * dref,  Failure_Action failure_action);
void         ddc_close_display(Display_Handle * dh);

Display_Ref* ddc_find_display_by_model_and_sn(const char * model, const char * sn);

Display_Ref* ddc_find_display_by_edid(const Byte * pEdidBytes);

Parsed_Edid* ddc_get_parsed_edid_by_display_handle(Display_Handle * dh);
Parsed_Edid* ddc_get_parsed_edid_by_display_ref(Display_Ref * dref);

bool         ddc_is_valid_display_ref(Display_Ref * dref, bool emit_error_msg);


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
       Display_Handle *  dh,
       DDC_Packet *      request_packet_ptr);


Global_Status_Code ddc_write_only_with_retry(
       Display_Handle *  dh,
       DDC_Packet *      request_packet_ptr);

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
         DDC_Packet *  request_packet_ptr,
         int           max_read_bytes,
         Byte          expected_response_type,
         Byte          expected_subtype,
         DDC_Packet ** response_packet_ptr_loc
        );

#endif /* DDC_PACKET_IO_H_ */
