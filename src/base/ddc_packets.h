/** \file ddc_packets.h
 * Functions for creating DDC packets and interpreting DDC response packets.
 */

// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_PACKETS_H_
#define DDC_PACKETS_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

#include "ddcutil_types.h"

#include "util/data_structures.h"

#include "base/status_code_mgt.h"
#include "base/core.h"

// was in common.h
#define MAX_DDCCI_PACKET_SIZE   37    //  32 + 5;

// largest packet is capabilities response packet, which has 1 byte for reply op code,
// 2 for offset, and up to 32 bytes fragment

#define MAX_DDC_DATA_SIZE           35
#define MAX_DDC_PACKET_WO_CHECKSUM  38
#define MAX_DDC_PACKET_INC_CHECKSUM 39

// also is max table fragment size
#define MAX_DDC_CAPABILITIES_FRAGMENT_SIZE 32

#define MAX_DDC_TAG 39

#ifdef NEW
// apparently unused
typedef struct {
   Byte    vcp_opcode;
   Byte    vcp_type_code;
   Byte    parse_status;
   Byte    mh;
   Byte    ml;
   Byte    sh;
   Byte    sl;
   void *  interpretation;     // specific to VCP_opcode
} Parsed_VCP_Response_Data;
#endif


/** Interpretation of a packet of type DDC_PACKET_TYPE_QUERY_VCP_RESPONSE */
typedef
struct {
   Byte   vcp_code;             ///< VCP feature code
   bool   valid_response;       ///<
   bool   supported_opcode;
   int    max_value;
   int    cur_value;
   // for new way
   Byte   mh;
   Byte   ml;
   Byte   sh;
   Byte   sl;
} Parsed_Nontable_Vcp_Response;


static inline bool
value_bytes_zero(Parsed_Nontable_Vcp_Response * parsed_val) {
   return (parsed_val->mh == 0 &&
           parsed_val->ml == 0 &&
           parsed_val->sh == 0 &&
           parsed_val->sl == 0);
}


/** For digesting capabilities or table read fragment */
typedef
struct {
   Byte  fragment_type;       // DDC_PACKET_TYPE_CAPABILITIES_RESPONSE || DDC_PACKET_TYPE_TABLE_READ_RESPONSE
   int   fragment_offset;
   int   fragment_length;     // without possible terminating '\0'
   // add 1 to allow for appending a terminating '\0' in case of DDC_PACKET_TYPE_CAPABILITIES_RESPONSE
   Byte  bytes[MAX_DDC_CAPABILITIES_FRAGMENT_SIZE+1];
} Interpreted_Multi_Part_Read_Fragment;


// TODO: Unify with list in ddc_command_codes.h
typedef Byte DDC_Packet_Type;
#define DDC_PACKET_TYPE_NONE                  0x00
#define DDC_PACKET_TYPE_QUERY_VCP_REQUEST     0x01
#define DDC_PACKET_TYPE_QUERY_VCP_RESPONSE    0x02
#define DDC_PACKET_TYPE_SET_VCP_REQUEST       0x03    // n. no reply message
#define DDC_PACKET_TYPE_SAVE_CURRENT_SETTINGS 0x0C    // n. no reply message
#define DDC_PACKET_TYPE_CAPABILITIES_REQUEST  0xf3
#define DDC_PACKET_TYPE_CAPABILITIES_RESPONSE 0xe3
#define DDC_PACKET_TYPE_ID_REQUEST            0xf1
#define DDC_PACKET_TYPE_ID_RESPONSE           0xe1
#define DDC_PACKET_TYPE_TABLE_READ_REQUEST    0xe2
#define DDC_PACKET_TYPE_TABLE_READ_RESPONSE   0xe4
#define DDC_PACKET_TYPE_TABLE_WRITE_REQUEST   0xe7

/** Packet bytes and interpretation */
typedef
struct {
   Buffer *         raw_bytes;                ///< raw packet bytes
   char             tag[MAX_DDC_TAG+1]; ///* debug string describing packet, +1 for \0
   DDC_Packet_Type  type;               ///* packet type
   // void *           aux_data;           ///* type dependent

   // for a bit more type safety and code clarity:
   union {
      Parsed_Nontable_Vcp_Response *         nontable_response;
      Interpreted_Multi_Part_Read_Fragment * multi_part_read_fragment;
      void *                                 raw_parsed;
   } parsed;

   // additional fields for new way of parsing result data
   // Parsed_Response_Data * parsed_response;
} DDC_Packet;

void dbgrpt_packet(DDC_Packet * packet, int depth);
void free_ddc_packet(DDC_Packet * packet);

bool is_double_byte(Byte * pb);

// Byte xor_bytes(Byte * bytes, int len);
Byte ddc_checksum(Byte * bytes, int len, bool altmode);
bool valid_ddc_packet_checksum(Byte * readbuf);
// void test_checksum();


typedef
struct {
   DDCA_Vcp_Value_Type             response_type;
   Parsed_Nontable_Vcp_Response *  non_table_response;
   Buffer *                        table_response;
} Parsed_Vcp_Response;

void dbgrpt_parsed_vcp_response(Parsed_Vcp_Response * response,int depth);

Status_DDC
create_ddc_base_response_packet(
      Byte *        i2c_response_bytes,
      int           response_bytes_buffer_size,
      const char *  tag,
      DDC_Packet ** packet_ptr);

Status_DDC
create_ddc_typed_response_packet(
      Byte *        i2c_response_bytes,
      int           response_bytes_buffer_size,
      Byte          expected_type,
      Byte          expected_subtype,
      const char *  tag,
      DDC_Packet ** packet_ptr_addr);

DDC_Packet *
create_ddc_capabilities_request_packet(
      int           offset,
      const char *  tag);

DDC_Packet *
create_ddc_multi_part_read_request_packet(
      Byte          request_type,
      Byte          request_subtype,
      int           offset,
      const char *  tag);

DDC_Packet *
create_ddc_multi_part_write_request_packet(
      Byte   request_type,     // always DDC_PACKET_TYPE_WRITE_REQUEST
      Byte   request_subtype,  // VCP code
      int    offset,
      Byte * bytes_to_write,
      int    bytect,
      const char * tag);

void
update_ddc_capabilities_request_packet_offset(
      DDC_Packet *  packet,
      int           offset);

void
update_ddc_multi_part_read_request_packet_offset(
      DDC_Packet *  packet,
      int           offset);

Status_DDC
create_ddc_capabilities_response_packet(
      Byte *        i2c_response_bytes,
      int           response_bytes_buffer_size,
      const char *  tag,
      DDC_Packet ** packet_ptr);

Status_DDC
interpret_capabilities_response(
      Byte *        data_bytes,
      int           bytect,
      Interpreted_Multi_Part_Read_Fragment * aux_data,
      bool          debug);

DDC_Packet *
create_ddc_getvcp_request_packet(
      Byte          vcp_code,
      const char *  tag);

Status_DDC
create_ddc_getvcp_response_packet(
      Byte *        i2c_response_bytes,
      int           response_bytes_buffer_size,
      Byte          expected_vcp_opcode,
      const char *  tag,
      DDC_Packet ** packet_ptr);

DDC_Packet *
create_ddc_setvcp_request_packet(
      Byte          vcp_code,
      int           new_value,
      const char *  tag);

DDC_Packet *
create_ddc_save_settings_request_packet(
      const char * tag);

Status_DDC
get_interpreted_vcp_code(
      DDC_Packet *  packet,
      bool          make_copy,
      Parsed_Nontable_Vcp_Response ** interpreted_loc);

void
dbgrpt_interpreted_multi_read_fragment(
      Interpreted_Multi_Part_Read_Fragment * interpreted,
      int depth);
void
dbgrpt_interpreted_nontable_vcp_response(
      Parsed_Nontable_Vcp_Response * interpreted,
      int depth);
#ifdef OLD
void   report_interpreted_aux_data(Byte response_type, void * interpreted);
#endif

Byte * get_packet_start(DDC_Packet * packet);
int    get_packet_len(  DDC_Packet * packet);
Byte * get_data_start(  DDC_Packet * packet);
int    get_data_len(    DDC_Packet * packet);



#endif /* DDC_PACKETS_H_ */
