/* ddc_packets.h
 *
 * Created on: Jun 10, 2014
 *     Author: rock
 *
 * Functions for creating DDC packets and interpreting DDC response packets.
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

#ifndef DDC_PACKETS_H_
#define DDC_PACKETS_H_

#include <stdbool.h>

#include "util/data_structures.h"

#include "base/status_code_mgt.h"
#include "base/util.h"

// was in common.h
#define MAX_DDCCI_PACKET_SIZE   37    //  32 + 5;

// largest packet is capabilities response packet, which has 1 byte for reply op code,
// 2 for offset, and up to 32 bytes fragment

#define MAX_DDC_DATA_SIZE           35
#define MAX_DDC_PACKET_WO_CHECKSUM  38
#define MAX_DDC_PACKET_INC_CHECKSUM 39

#define MAX_DDC_CAPABILITIES_FRAGMENT_SIZE 32

#define MAX_DDC_TAG 39

void init_ddc_packets();

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

typedef
struct {
   Buffer * buf;
   char     tag[MAX_DDC_TAG+1];    // +1 for \0
   Byte     type;
   void *   aux_data;      // type dependent
   // additional fields for new way of parsing result data
   // Parsed_Response_Data * parsed_response;
} DDC_Packet;

void dump_packet(DDC_Packet * packet);

bool is_double_byte(Byte * pb);

// TODO: Unify with list in ddc_command_codes.h

#define DDC_PACKET_TYPE_NONE                  0x00
#define DDC_PACKET_TYPE_QUERY_VCP_REQUEST     0x01
#define DDC_PACKET_TYPE_QUERY_VCP_RESPONSE    0x02
#define DDC_PACKET_TYPE_SET_VCP_REQUEST       0x03    // n. no reply message
#define DDC_PACKET_TYPE_CAPABILITIES_REQUEST  0xf3
#define DDC_PACKET_TYPE_CAPABILITIES_RESPONSE 0xe3
#define DDC_PACKET_TYPE_ID_REQUEST            0xf1
#define DDC_PACKET_TYPE_ID_RESPONSE           0xe1
#define DDC_PACKET_TYPE_TABLE_READ_REQUEST    0xe2
#define DDC_PACKET_TYPE_TABLE_READ_RESPONSE   0xe4
#define DDC_PACKET_TYPE_TABLE_WRITE_REQUEST   0xe7
#define DDC_PACKET_TYPE_

Byte xor_bytes(Byte * bytes, int len);
Byte ddc_checksum(Byte * bytes, int len, bool altmode);
bool valid_ddc_packet_checksum(Byte * readbuf);
void test_checksum();


typedef
struct {
   Byte   vcp_code;
   bool   valid_response;
   bool   supported_opcode;
   int    max_value;
   int    cur_value;
   // for new way
   Byte   mh;
   Byte   ml;
   Byte   sh;
   Byte   sl;
} Parsed_Nontable_Vcp_Response;



typedef enum {NON_TABLE_VCP_CALL,
              TABLE_VCP_CALL,
             } VCP_Call_Type;


typedef
struct {
   VCP_Call_Type                     response_type;
   Parsed_Nontable_Vcp_Response * non_table_response;
   Buffer *                          table_response;
} Parsed_Vcp_Response;


typedef
struct {
   int   fragment_offset;
   int   fragment_length_wo_null;
   char  text[MAX_DDC_CAPABILITIES_FRAGMENT_SIZE+1];    // max fragment size + 1 for null
} Interpreted_Capabilities_Fragment;

typedef
struct {
   int   fragment_offset;
   int   fragment_length;
   Byte  read_data[MAX_DDC_CAPABILITIES_FRAGMENT_SIZE];
} Interpreted_Table_Read_Fragment;


typedef
struct {
   Byte  fragment_type;       // DDC_PACKET_TYPE_CAPABILITIES_RESPONSE || DDC_PACKET_TYPE_TABLE_READ_RESPONSE
   int   fragment_offset;
   int   fragment_length;
   Byte  bytes[MAX_DDC_CAPABILITIES_FRAGMENT_SIZE];
} Interpreted_Multi_Part_Read_Fragment;




void   free_ddc_packet(DDC_Packet * packet);

Global_Status_DDC create_ddc_base_response_packet(
          Byte *        i2c_response_bytes,
          int           response_bytes_buffer_size,
          const char *  tag,
          DDC_Packet ** packet_ptr);
Global_Status_DDC create_ddc_typed_response_packet(
          Byte *        i2c_response_bytes,
          int           response_bytes_buffer_size,
          Byte          expected_type,
          Byte          expected_subtype,
          const char *  tag,
          DDC_Packet ** packet_ptr_addr);
DDC_Packet *      create_ddc_capabilities_request_packet(
          int           offset,
          const char *  tag);
DDC_Packet *      create_ddc_multi_part_read_request_packet(
          Byte          request_type,
          Byte          request_subtype,
          int           offset,
          const char *  tag);


void    update_ddc_capabilities_request_packet_offset(
          DDC_Packet *  packet,
          int           offset);
void    update_ddc_multi_part_read_request_packet_offset(
          DDC_Packet *  packet,
          int           offset);
Global_Status_DDC create_ddc_capabilities_response_packet(
          Byte *        i2c_response_bytes,
          int           response_bytes_buffer_size,
          const char *  tag,
          DDC_Packet ** packet_ptr);
Global_Status_DDC interpret_capabilities_response(
          Byte *        data_bytes,
          int           bytect,
          Interpreted_Capabilities_Fragment * aux_data,
          bool          debug);


DDC_Packet * create_ddc_getvcp_request_packet(
          Byte          vcp_code,
          const char *  tag);

Global_Status_DDC create_ddc_getvcp_response_packet(
          Byte *        i2c_response_bytes,
          int           response_bytes_buffer_size,
          Byte          expected_vcp_opcode,
          const char *  tag,
          DDC_Packet ** packet_ptr);

DDC_Packet * create_ddc_setvcp_request_packet(
          Byte          vcp_code,
          int           new_value,
          const char *  tag);
Global_Status_DDC get_interpreted_vcp_code(
          DDC_Packet *  packet,
          bool          make_copy,
          Parsed_Nontable_Vcp_Response ** interpreted_ptr);

void   report_interpreted_capabilities(Interpreted_Capabilities_Fragment * interpreted);
void   report_interpreted_multi_read_fragment(Interpreted_Multi_Part_Read_Fragment * interpreted);
void   report_interpreted_nontable_vcp_response(Parsed_Nontable_Vcp_Response * interpreted);
void   report_interpreted_aux_data(Byte response_type, void * interpreted);

Byte * get_packet_start(DDC_Packet * packet);
int    get_packet_len(  DDC_Packet * packet);
Byte * get_data_start(  DDC_Packet * packet);
int    get_data_len(    DDC_Packet * packet);



#endif /* DDC_PACKETS_H_ */
