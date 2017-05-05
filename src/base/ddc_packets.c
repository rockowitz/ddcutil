/* ddc_packets.c
 *
 * Functions for creating DDC packets and interpreting DDC response packets.
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
 * Functions for creating DDC packets and interpreting DDC response packets.
 */

/** \cond */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "util/report_util.h"
#include "util/string_util.h"

#include "base/ddc_errno.h"
#include "base/execution_stats.h"

#include "base/ddc_packets.h"


//
//  Initialization
//

void init_ddc_packets() {
}


//
// Trace control
//

Trace_Group TRACE_GROUP = TRC_DDC;


//
// Utilities
//

/* Tests if a byte is equal to the preceding byte
 *
 * Arguments:   pb   pointer to byte (test this byte and *(pb-1)
 *
 * Returns:
 *    true/false
 */
bool is_double_byte(Byte * pb) {
   Byte * pb0 = pb-1;
   // DBGMSG("pb=%p, pb0=%p, *pb=0x%02x *pb0=0x%02x ", pb, pb0, *pb, *pb0 );
   bool result = (*pb == *pb0);
   // DBGMSG("Returning: %d  ", result );
   return result;
}


//
// Checksums
//

Byte xor_bytes(Byte * bytes, int len) {
   Byte result = 0x00;
   int  ndx;

   for (ndx=0; ndx < len; ndx++) {
      result = result ^ bytes[ndx];
   }
   return result;
}


Byte ddc_checksum(Byte * bytes, int len, bool altmode) {
   // DBGMSG("bytes=%p, len=%d, altmode=%d", bytes, len, altmode);
   // largest packet is capabilities fragment, which can have up to 32 bytes of text,
   // plus 4 bytes of offset data.  Adding this to the dest, src, and len bytes is 39
   assert(len <= MAX_DDC_PACKET_WO_CHECKSUM);
   Byte workbuf[MAX_DDC_PACKET_WO_CHECKSUM];
   Byte * bytes2 = bytes;
   Byte result;

   if (altmode) {
      bytes2 = workbuf;
      memcpy(bytes2, bytes, len);
      bytes2[0] = 0x50;
   }
   result = xor_bytes(bytes2, len);

   // char * hs = hexstring(bytes2, len);
   // DBGMSG("checksum for %s is 0x%02x", hs, result);
   // free(hs);

   return result;
}


void test_one_checksum(Byte * bytes, int len, bool altmode, Byte expected, char * spec_section) {
   unsigned char actual = ddc_checksum(bytes, len, altmode);
   char * hs =   hexstring(bytes, len);
   printf( "bytes=%s, altmode=%d, expected=0x%02x, actual=0x%02x, spec section=%s\n",
      hs,
      altmode,
      expected, actual, spec_section
     );
   free(hs);
}


void test_checksum() {
   puts("\ntest_checksum\n");
   Byte bytes[] = {0x6e, 0x51, 0x82, 0xf5, 0x01};
   test_one_checksum( bytes,                                   5, false, 0x49, "6.2" );
   test_one_checksum( (Byte[]) {0x6e, 0x51, 0x81, 0xb1},       4, false, 0x0f, "6.3" );
   test_one_checksum( (Byte[]) {0x6f, 0x6e, 0x82, 0xa1, 0x00}, 5, true,  0x1d, "6.3" );
   test_one_checksum( (Byte[]) {0x6f, 0x6e, 0x80},             3, true,  0xbe, "6.4" );
   test_one_checksum( (Byte[]) {0xf0, 0xf1, 0x81, 0xb1},       4, false, 0x31, "7.4" );
   test_one_checksum( (Byte[]) {0x6e, 0xf1, 0x81, 0xb1},       4, false, 0xaf, "7.4" );
   test_one_checksum( (Byte[]) {0xf1, 0xf0, 0x82, 0xa1, 0x00}, 5, true,  0x83, "7.4");
   test_one_checksum( (Byte[]) {0x6f, 0xf0, 0x82, 0xa1, 0x00}, 5, true,  0x83, "7.4");
}


bool valid_ddc_packet_checksum(Byte * readbuf) {
   bool result;

   int data_size = (readbuf[2] & 0x7f);
   // DBGMSG("data_size = %d", data_size);
   if (data_size > MAX_DDCCI_PACKET_SIZE) {    // correct constant?
      // DBGMSG("Invalid data_size = %d", data_size);
      DDCMSG("Invalid data_size = %d", data_size);
      result = false;
   }
   else {
      int response_size_wo_checksum = 3 + data_size;
      readbuf[1] = 0x51;   // dangerous
      unsigned char expected_checksum = ddc_checksum(readbuf, response_size_wo_checksum, false);
      unsigned char actual_checksum   = readbuf[response_size_wo_checksum];
      // printf("(%s) actual checksum = 0x%02x, expected = 0x%02x\n",
      //        __func__, actual_checksum, expected_checksum);
      TRCMSG("actual checksum = 0x%02x, expected = 0x%02x",
             actual_checksum, expected_checksum);
      result = (expected_checksum == actual_checksum);
   }

   // DBGMSG("Returning: %d", result);
   return result;
}


//
//  Packet general functions 
//

Byte * get_packet_start(DDC_Packet * packet) {
   Byte * result = NULL;
   if (packet)
      result = packet->raw_bytes->bytes;
   return result;
}


int get_packet_len(DDC_Packet * packet) {
   return (packet) ? packet->raw_bytes->len : 0;
}


int get_data_len(DDC_Packet * packet) {
   return (packet) ? packet->raw_bytes->len - 4 : 0;
}


Byte * get_data_start(DDC_Packet * packet) {
   return (packet) ? packet->raw_bytes->bytes+3 : NULL;
}


int get_packet_max_size(DDC_Packet * packet) {
   return packet->raw_bytes->buffer_size;
}


void dump_packet(DDC_Packet * packet) {
   assert(packet);      // make clang analyzer happy
   // printf("DDC_Packet dump.  Addr: %p, Type: 0x%02x, Tag: |%s|, buf: %p, aux_data: %p\n",
   //        packet, packet->type, packet->tag, packet->raw_bytes, packet->aux_data);
   printf("DDC_Packet dump.  Addr: %p, Type: 0x%02x, Tag: |%s|, buf: %p, parsed: %p\n",
          packet, packet->type, packet->tag, packet->raw_bytes, packet->parsed.raw_parsed);
   buffer_dump(packet->raw_bytes);
   // TODO show interpreted aux_data
#ifdef OLD
   if (packet->aux_data)
      report_interpreted_aux_data(packet->type, packet->aux_data);
#endif
   if (packet->parsed.raw_parsed) {
      switch(packet->type) {
      case (DDC_PACKET_TYPE_CAPABILITIES_RESPONSE):
      case (DDC_PACKET_TYPE_TABLE_READ_RESPONSE):
         report_interpreted_multi_read_fragment(packet->parsed.multi_part_read_fragment, 0);
         break;
      case (DDC_PACKET_TYPE_QUERY_VCP_RESPONSE):
         report_interpreted_nontable_vcp_response(packet->parsed.nontable_response, 0);
         break;
      default:
         PROGRAM_LOGIC_ERROR("Unexpected packet type: -x%02x", packet->type);
      }
   }
}

#ifdef OLD
void report_interpreted_aux_data(Byte response_type, void * aux_data) {
   assert(aux_data);

   printf("Interpreting aux data at %p for response type: 0x%02x\n", aux_data, response_type);
   switch (response_type) {
   case (DDC_PACKET_TYPE_CAPABILITIES_RESPONSE):
   case (DDC_PACKET_TYPE_TABLE_READ_RESPONSE):
      report_interpreted_multi_read_fragment(aux_data);
      break;
   case (DDC_PACKET_TYPE_QUERY_VCP_RESPONSE):
      report_interpreted_nontable_vcp_response(aux_data, 0);
      break;

   default:
      DBGMSG("Don't know how to interpret aux data for response type: 0x%02x", response_type);
   }
}
#endif



bool isNullPacket(DDC_Packet * packet) {
   return  (get_data_len(packet) == 0);
}


void free_ddc_packet(DDC_Packet * packet) {
   bool debug = false;
   DBGMSF(debug, "packet=%p", packet);

   // dump_packet(packet);

   if (packet) {
#ifdef OLD
      if (packet->aux_data) {
         DBGMSF(debug, "freeing packet->aux_data=%p", packet->aux_data);
         free(packet->aux_data);
      }
#endif
      if (packet->parsed.raw_parsed) {
         DBGMSF(debug, "freeing packet->parsed.raw=%p", packet->parsed.raw_parsed);
         free(packet->parsed.raw_parsed);
      }

      DBGMSF(debug, "calling free_buffer() for packet->buf=%p", packet->raw_bytes);
      buffer_free(packet->raw_bytes, "free DDC packet");

      DBGMSF(debug, "freeing packet=%p", packet);
      free(packet);
   }
   DBGMSF(debug, "Done" );
}


/** Base function for creating any DDC packet
 *
 *  \param  max_size  size of buffer allocated for packet bytes
 *  \param  tag       debug string (may be NULL)
 *
 *  \return pointer to newly allocated #DDC_Packet
 */
DDC_Packet * create_empty_ddc_packet(int max_size, const char * tag) {
   bool debug = false;
   DBGMSF(debug, "Starting. max_size=%d, tag=%s", max_size, (tag) ? tag : "(nil)");

   DDC_Packet * packet = malloc(sizeof(DDC_Packet));
   packet->raw_bytes = buffer_new(max_size, "empty DDC packet");
   if (tag) {
      g_strlcpy(packet->tag, tag, MAX_DDC_TAG);
   }
   else
      packet->tag[0] = '\0';
   // DBGMSG("packet->tag=%s", packet->tag);
   packet->type = DDC_PACKET_TYPE_NONE;
   packet->parsed.raw_parsed = NULL;

   DBGMSF(debug, "Done. Returning %p, packet->tag=%p", packet, packet->tag);
   if (debug)
      dump_packet(packet);

   return packet;
}


//
// Request Packets
//

/** Creates a generic DDC request packet
 *
 *  \param  data_bytes   data bytes of packet
 *  \param  data_bytect  number of data bytes
 *  \param  tag          debug string (may be NULL)
 *
 *  \return  pointer to created packet
 */
DDC_Packet * create_ddc_base_request_packet(
                Byte *       data_bytes,
                int          data_bytect,
                const char * tag)
{
   bool debug = false;
   if (debug) {
      char * hs =  hexstring(data_bytes, data_bytect);
      DBGMSG("Starting.  bytes=%s, tag=%s", hs, tag);
      free(hs);
   }

   assert( data_bytect <= 32 );

   DDC_Packet * packet = create_empty_ddc_packet(3+data_bytect+1, tag);
   buffer_set_byte( packet->raw_bytes, 0, 0x6e);
   buffer_set_byte( packet->raw_bytes, 1, 0x51);
   buffer_set_byte( packet->raw_bytes, 2, data_bytect | 0x80);
   buffer_set_bytes(packet->raw_bytes, 3, data_bytes, data_bytect);
   int packet_size_wo_checksum = 3 + data_bytect;
   Byte checksum = ddc_checksum(packet->raw_bytes->bytes, packet_size_wo_checksum, false);
   buffer_set_byte(packet->raw_bytes, packet_size_wo_checksum, checksum);
   buffer_set_length(packet->raw_bytes, 3 + data_bytect + 1);
   if (data_bytect > 0)
      packet->type = data_bytes[0];
   else
      packet->type = 0x00;
   // dump_buffer(packet->buf);

   DBGMSF(debug, "Done. packet=%p", packet);
   return packet;
}


/* Creates a DDC VCP table read request packet
 *
 * Arguments:
 *    offset  offset value
 *    tag     debug string
 *
 * Returns:
 *    pointer to created capabilities request packet
 */
DDC_Packet * create_ddc_multi_part_read_request_packet(
                Byte request_type,
                Byte request_subtype,
                int offset,
                const char * tag)
{
   assert (request_type == DDC_PACKET_TYPE_CAPABILITIES_REQUEST ||
           request_type == DDC_PACKET_TYPE_TABLE_READ_REQUEST );
   DDC_Packet * packet_ptr = NULL;

   Byte ofs_hi_byte = (offset >> 16) & 0xff;
   Byte ofs_lo_byte = offset & 0xff;

   if (request_type == DDC_PACKET_TYPE_CAPABILITIES_REQUEST) {
      Byte data_bytes[] = { DDC_PACKET_TYPE_CAPABILITIES_REQUEST ,
                              ofs_hi_byte,
                              ofs_lo_byte
                            };
        packet_ptr = create_ddc_base_request_packet(data_bytes, 3, tag);
   }
   else {
      Byte data_bytes[] = { DDC_PACKET_TYPE_TABLE_READ_REQUEST,
                            request_subtype,    // VCP code
                            ofs_hi_byte,
                            ofs_lo_byte
                          };
      packet_ptr = create_ddc_base_request_packet(data_bytes, 4, tag);
   }

   // DBGMSG("Done. packet_ptr=%p", packet_ptr);
   // dump_packet(packet_ptr);
   return packet_ptr;
}


/* Updates the offset in a multi part read request packet
 *
 * Arguments:
 *    packet  address of packet
 *    offset  new offset value
 */
void update_ddc_multi_part_read_request_packet_offset(DDC_Packet * packet, int new_offset) {
   assert (packet->type == DDC_PACKET_TYPE_CAPABILITIES_REQUEST ||
           packet->type == DDC_PACKET_TYPE_TABLE_READ_REQUEST );

   // update offset
   Byte ofs_hi_byte = (new_offset >> 8) & 0xff;
   // ofs_hi_byte = 0x00;                             // *** TEMP *** INSERT BUG
   Byte ofs_lo_byte = new_offset & 0xff;

   Byte * data_bytes = get_data_start(packet);
   if (packet->type == DDC_PACKET_TYPE_CAPABILITIES_REQUEST) {
      data_bytes[1] = ofs_hi_byte;
      data_bytes[2] = ofs_lo_byte;
   }
   else {
      data_bytes[2] = ofs_hi_byte;   // changed from update_ddc_capabilities_request_offset
      data_bytes[3] = ofs_lo_byte;   // changed ...
   }
   // DBGMSG("offset=%d, ofs_hi_byte=0x%02x, ofs_lo_byte=0x%02x", new_offset, ofs_hi_byte, ofs_lo_byte );

   // update checksum
   Byte * bytes = get_packet_start(packet);
   int packet_size_wo_checksum = get_packet_len(packet)-1;
   bytes[packet_size_wo_checksum] = ddc_checksum(bytes, packet_size_wo_checksum, false);

   // DBGMSG("Done.");
   // dump_packet(packet);
}


/* Creates a DDC VCP table write request packet
 *
 * Arguments:
 *    offset  offset value
 *    tag     debug string
 *
 * Returns:
 *    pointer to created capabilities request packet
 */
DDC_Packet * create_ddc_multi_part_write_request_packet(
                Byte   request_type,     // always DDC_PACKET_TYPE_WRITE_REQUEST
                Byte   request_subtype,  // VCP code
                int    offset,
                Byte * bytes_to_write,
                int    bytect,
                const char * tag)
{
   assert (request_type ==  DDC_PACKET_TYPE_TABLE_WRITE_REQUEST );
   assert (bytect + 4 <= 35);    // is this the right limit?, spec unclear
   DDC_Packet * packet_ptr = NULL;

   Byte ofs_hi_byte = (offset >> 16) & 0xff;
   Byte ofs_lo_byte = offset & 0xff;


   Byte data_bytes[40] = { DDC_PACKET_TYPE_TABLE_WRITE_REQUEST,
                           request_subtype,    // VCP code
                           ofs_hi_byte,
                           ofs_lo_byte
                         };
   memcpy(data_bytes+4, bytes_to_write, bytect);
   packet_ptr = create_ddc_base_request_packet(data_bytes, 4+bytect, tag);

   // DBGMSG("Done. packet_ptr=%p", packet_ptr);
   // dump_packet(packet_ptr);
   return packet_ptr;
}


/* Creates a Get VCP request packet
 *
 * Arguments:
 *    vcp_code  VCP feature code
 *    tag       debug string
 *
 * Returns:
 *    pointer to created DDC packet
 */
DDC_Packet *
create_ddc_getvcp_request_packet(Byte vcp_code, const char * tag)
{
   Byte data_bytes[] = { 0x01,     // Command: get VCP Feature
                         vcp_code  // VCP opcode
                       };
   DDC_Packet * pkt = create_ddc_base_request_packet(data_bytes, 2, tag);

   // DBGMSG("Done. rc=%d, packet_ptr%p, *packet_ptr=%p", rc, packet_ptr, *packet_ptr);
   return pkt;
}


/* Creates a Set VCP request packet
 *
 * Arguments:
 *    vcp_code  VCP feature code
 *    int       new value
 *    tag       debug string
 *
 * Returns:
 *    pointer to created DDC packet
 */
DDC_Packet *
create_ddc_setvcp_request_packet(Byte vcp_code, int new_value, const char * tag)
{
   Byte data_bytes[] = { 0x03,   // Command: get VCP Feature
                         vcp_code,  // VCP opcode
                         (new_value >> 8) & 0xff,
                         new_value & 0xff
                       };
   DDC_Packet * pkt = create_ddc_base_request_packet(data_bytes, 4, tag);

   // DBGMSG("Done. rc=%d, packet_ptr%p, *packet_ptr=%p", rc, packet_ptr, *packet_ptr);
   return pkt;
}


DDC_Packet *
create_ddc_save_settings_request_packet(const char * tag)
{
   Byte data_bytes[] = { 0x0C   // Command: Save Current Settings
                       };
   DDC_Packet * pkt = create_ddc_base_request_packet(data_bytes, 1, tag);

   // DBGMSG("Done. rc=%d, packet_ptr%p, *packet_ptr=%p", rc, packet_ptr, *packet_ptr);
   return pkt;
}


//
// Response Packets
//

/** Performs tasks common to creating any DDC response packet.
 *  Checks for malformed packet, but not packet contents.
 *
 *  \param i2c_response_bytes          pointer to raw packet bytes
 *  \param response_bytes_buffer_size  size of buffer pointed to by **i2c_response_bytes**,
 *                                     (used for debug hex dump)
 *  \param tag                         debug string (may be NULL)
 *  \param packet_ptr_addr             where to return pointer to newly allocated #DDC_Packet
 *
 *  \retval 0
 *  \retval DDCRC_RESPONSE_ENVELOPE
 *  \retval DDCRC_DOUBLE_BYTE
 *  \retval DDCRC_PACKET_SIZE
 *  \retval DDCRC_CHECKSUM
 *
 *  The pointer returned at packet_ptr_addr is non-null iff the status code is 0.
 */
Status_DDC
create_ddc_base_response_packet(
   Byte *        i2c_response_bytes,
   int           response_bytes_buffer_size,
   const char *  tag,
   DDC_Packet ** packet_ptr_addr)
{
   bool debug = false;
   if (debug) {
      char * hs = hexstring(i2c_response_bytes,20);
      DBGMSG("Starting. i2c_response_bytes=%s", hs );
      free(hs);
   }

   int result = DDCRC_OK;
   DDC_Packet * packet = NULL;
   if (i2c_response_bytes[0] != 0x6e ) {
      // DBGMSG("Unexpected source address 0x%02x, should be 0x6e", i2c_response_bytes[0]);
      DDCMSG("Unexpected source address 0x%02x, should be 0x6e", i2c_response_bytes[0]);
      result = DDCRC_RESPONSE_ENVELOPE;
   }
   else {
      int data_ct = i2c_response_bytes[1] & 0x7f;
      // DBGMSG("data_ct=%d", data_ct);
      if (data_ct > MAX_DDC_DATA_SIZE) {
         if ( is_double_byte(&i2c_response_bytes[1])) {
            result = DDCRC_DOUBLE_BYTE;
            // if (showRecoverableErrors || debug)
            //    DBGMSG("Double byte in packet.");
            DDCMSG("Double byte in packet.");
         }
         else {
            result = DDCRC_PACKET_SIZE;
            // if (showRecoverableErrors || debug)
            //    printf("(%s) Invalid data length in packet: %d exceeds MAX_DDC_DATA_SIZE\n",
            //           __func__, data_ct);
            DDCMSG("Invalid data length in packet: %d exceeds MAX_DDC_DATA_SIZE", data_ct);
         }
      }
      else {
         packet = create_empty_ddc_packet(3 + data_ct + 1, tag);
         // DBGMSG("create_empty_ddc_packet() returned %p", packet);
         if (data_ct > 0)
            packet->type = i2c_response_bytes[2];
         Byte * packet_bytes = packet->raw_bytes->bytes;
         // DBGMSG("packet_bytes=%p", packet_bytes);
         // packet_bytes[0] = 0x6f;    // implicit, would be 0x50 on access bus
         // packet_bytes[1] = 0x6e;       // i2c_response_bytes[0[
         // memcpy(packet_bytes+2, i2c_response_bytes+1, 1 + data_ct + 1);
         // packet->buf->len = 3 + data_ct + 1;
         buffer_set_byte(  packet->raw_bytes, 0, 0x6f);     // implicit, would be 0x50 on access bus
         buffer_set_byte(  packet->raw_bytes, 1, 0x6e);     // i2c_response_bytes[0]
         buffer_set_bytes( packet->raw_bytes, 2, i2c_response_bytes+1, 1 + data_ct + 1);
         buffer_set_length(packet->raw_bytes, 3 + data_ct + 1);
         Byte calculated_checksum = ddc_checksum(packet_bytes, 3 + data_ct, true);   // replacing right byte?
         Byte actual_checksum = packet_bytes[3+data_ct];
         if (calculated_checksum != actual_checksum) {
            // if (showRecoverableErrors || debug)
            //    printf("(%s) Actual checksum 0x%02x, expected 0x%02x\n",
            //           __func__, actual_checksum, calculated_checksum);
            DDCMSG("Actual checksum 0x%02x, expected 0x%02x",
                   actual_checksum, calculated_checksum);
            // DBGMSG("!!! SUPPRESSING CHECKSUM ERROR");
            result = DDCRC_CHECKSUM;
            // DBGMSG("Freeing packet=%p", packet);
            free_ddc_packet(packet);
         }
      }
   }

   if (result != DDCRC_OK) {
      // if (showRecoverableErrors || debug) {
      //    printf("(%s) i2c_response_bytes: %s\n",
      //           __func__,  hexstring(i2c_response_bytes, response_bytes_buffer_size));
      // }
      char * hs = hexstring(i2c_response_bytes, response_bytes_buffer_size);
      DDCMSG("i2c_response_bytes: %s", hs);
      free(hs);
   }

   if (result == DDCRC_OK)
      *packet_ptr_addr = packet;
   else
      *packet_ptr_addr = NULL;

   DBGMSF(debug, "Returning %s, *packet_ptr_addr=%p\n", ddcrc_desc(result), *packet_ptr_addr);

   assert( (result==DDCRC_OK && *packet_ptr_addr) || (result != DDCRC_OK && !*packet_ptr_addr));
   return result;
}


/** Creates a DDC response packet, checking for expected type and DDC Null Response
 *
 *  \param i2c_response_bytes          pointer to raw packet bytes
 *  \param response_bytes_buffer_size  size of buffer pointed to by **i2c_response_bytes**
 *  \param expected_type               expected packet type
 *  \param tag                         debug string (may be NULL)
 *  \param packet_ptr_addr             where to return pointer to newly allocated #DDC_Packet
 *
 *  \return 0 for success\n
 *          as from create_ddc_base_response_packet, indicating malformed response
 *  \retval DDCRC_NULL_RESPONSE
 *  \retval DDCRC_RESPONSE_TYPE
 *
 *  The pointer returned at packet_ptr_addr is non-null iff the status code is 0.
 */
Status_DDC
create_ddc_response_packet(
       Byte *          i2c_response_bytes,
       int             response_bytes_buffer_size,
       DDC_Packet_Type expected_type,
       const char *    tag,
       DDC_Packet **   packet_ptr_addr)
{
   bool debug = false;
   if (debug) {
      char * hs = hexstring(i2c_response_bytes,20);
      DBGMSG("Starting. i2c_response_bytes=%s", hs);
      free(hs);
   }

   Status_DDC result = create_ddc_base_response_packet(
                          i2c_response_bytes,
                          response_bytes_buffer_size,
                          tag,
                          packet_ptr_addr);
   // DBGMSG("create_ddc_base_response_packet() returned %d, *packet_ptr_addr=%p", result, *packet_ptr_addr);
   if (result == 0) {
      if (isNullPacket(*packet_ptr_addr)) {
         result = DDCRC_NULL_RESPONSE;
      }
      else if ( get_data_start(*packet_ptr_addr)[0] != expected_type) {
         result = DDCRC_RESPONSE_TYPE;
      }
   }

   if (result != DDCRC_OK && *packet_ptr_addr) {
      // if (debug)
      //    DBGMSG("failure, freeing response packet at %p", *packet_ptr_addr);
      TRCMSG("failure, freeing response packet at %p", *packet_ptr_addr);
      // does this cause the free(readbuf) failure in try_read?
      free_ddc_packet(*packet_ptr_addr);
      *packet_ptr_addr = 0;
   }

   if (result < 0) {
      log_status_code(result, __func__);
   }

   DBGMSF(debug, "Returning %s, *packet_ptr_addr=%p", ddcrc_desc(result), *packet_ptr_addr);
   assert( (result==DDCRC_OK && *packet_ptr_addr) || (result != DDCRC_OK && !*packet_ptr_addr));
   return result;
}


//
// Packet data parsers
//

// Capabilities response data 

void
report_interpreted_capabilities(
      Interpreted_Multi_Part_Read_Fragment * interpreted,
      int depth)
{
   int d1 = depth+1;
   rpt_vstring(depth, "Capabilities response contents:");
   rpt_vstring(d1,    "offset:          %d", interpreted->fragment_offset);
   rpt_vstring(d1,    "fragment length: %d", interpreted->fragment_length);
   rpt_vstring(d1,    "text:            |%.*s|", interpreted->fragment_length, interpreted->bytes);
}


/** Interprets the bytes of a multi part read response.
 *
 *  \param  response_type
 *  \param  data_bytes
 *  \param  bytect
 *  \param  aux_data       pointer to #Interpreted_Multi_Part_Read_Fragment to fill in
 *  \retval 0 success
 *  \retval  DDCRC_INVALID_DATA
 */
Status_DDC
interpret_multi_part_read_response(
       DDC_Packet_Type  response_type,
       Byte * data_bytes,
       int    bytect,
       Interpreted_Multi_Part_Read_Fragment * aux_data)  //   Interpreted_Capabilities_Fragment * aux_data,
{
   bool debug = false;
   int result = DDCRC_OK;

   // not needed, already checked
   if (bytect < 3 || bytect > 35) {
      // if (debug)
      // DBGMSG("Invalid response data length: %d", bytect);
      DDCMSG("(DDCMSG) Invalid response data length: %d", bytect);
      result = COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
   }
   else {
      assert( data_bytes[0] == response_type);             // table read reply opcode    // CHANGED
      Byte   offset_hi_byte = data_bytes[1];
      Byte   offset_lo_byte = data_bytes[2];
      int    read_data_length = bytect-3;    // max 32  // CHANGED
      Byte * read_data_start = data_bytes+3;    // CHANGED

      // DBGMSG("offset_hi_byte = 0x%02x, offset_lo_byte = 0x%02x", offset_hi_byte, offset_lo_byte );
      aux_data->fragment_type = response_type;    // set in caller?  would make response_type parm unnecessary
      aux_data->fragment_offset = offset_hi_byte << 8 | offset_lo_byte;
      aux_data->fragment_length = read_data_length;      // changed
      assert(read_data_length <= MAX_DDC_CAPABILITIES_FRAGMENT_SIZE);   // ???
      memcpy(aux_data->bytes, read_data_start, read_data_length);    // CHANGED
      // aux_data->text[text_length] = '\0';     // CHANGED
   }
   if (debug)
      DBGMSG("returning %s", ddcrc_desc(result));
   return result;
}


void
report_interpreted_multi_read_fragment(
      Interpreted_Multi_Part_Read_Fragment * interpreted,
      int depth)
{
   int d1 = depth+1;
   rpt_vstring(depth, "Multi-read response contents:");
   rpt_vstring(d1,    "fragment type:   0x%02x", interpreted->fragment_type);
   rpt_vstring(d1,    "offset:          %d",     interpreted->fragment_offset);
   rpt_vstring(d1,    "fragment length: %d",     interpreted->fragment_length);
   rpt_vstring(d1,    "data addr:       %p",     interpreted->bytes);
   if (interpreted->fragment_type == DDC_PACKET_TYPE_CAPABILITIES_RESPONSE)
   rpt_vstring(d1,    "text:            |%.*s|", interpreted->fragment_length, interpreted->bytes);
   else {
      char * hs = hexstring(interpreted->bytes,  interpreted->fragment_length);
      rpt_vstring(d1, "data:            0x%s", hs);
      free(hs);
   }
}


//  VCP feature response data 

// overlay the standard 8 byte VCP feature response
typedef
 // no benefit to union here
 // union /* Vcp_Response */ {
 //            Byte bytes[8];
           struct {
              Byte  feature_reply_op_code;    // always 0x02
              Byte  result_code;              // 0x00=no error, 0x01=Unsupported op code
              Byte  vcp_opcode;               // VCP opcode from feature request message
              Byte  vcp_typecode;             // 0x00=set parameter, 0x01=momentary
              Byte  max_val_hi_byte;
              Byte  max_val_lo_byte;
              Byte  cur_val_hi_byte;
              Byte  cur_val_lo_byte;
//            } fields;
        } /*__attribute__((packed)) */ Vcp_Response;


/** Interprets the standard 8 byte VCP feature response.
 *
 * The response is checked for validity, and a
 * Interpreted_Notable_Vcp_Response struct is filled in.
 *
 * \param  vcp_data_bytes      pointer to data bytes
 * \param  bytect              number of bytes in response, must be 8
 * \param  requested_vcp_code  must be in the vcp_code field of he response bytes
 * \param  aux_data            pointer to #Parsed_Nontable_Vcp_Response struct to be filled in
 *
 * \retval 0    success
 * \retval DDCRC_INVALID_DATA
 *
 * \remark
 * It is not an error if the supported_opcode byte is set false in an
 * otherwise well constructed response.
 */
Status_DDC
interpret_vcp_feature_response_std(
       Byte*                 vcp_data_bytes,
       int                   bytect,
       Byte                  requested_vcp_code,
       Parsed_Nontable_Vcp_Response* aux_data)   // record in which interpreted feature response will be stored
{
   bool debug = false;
   DBGMSF(debug, "Starting.");
   if (debug) {
      DBGMSG("vcp_data_bytes: ");
      rpt_hex_dump(vcp_data_bytes, bytect, 1);
   }

   int result = DDCRC_OK;
   // set initial values for failure case:
   aux_data->vcp_code         = 0x00;
   aux_data->valid_response   = false;
   aux_data->supported_opcode = false;
   aux_data->max_value        = 0;
   aux_data->cur_value        = 0;

   if (bytect != 8) {
      // DBGMSF(debug, "Invalid response data length: %d, should be 8", bytect);
      DDCMSG("(DDCMSG) Invalid response data length: %d, should be 8", bytect);
      COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
      result = DDCRC_INVALID_DATA;
   }
   else {
      // overlay Vcp_Response on the data bytes of the response
      Vcp_Response * vcpresp = (Vcp_Response *) vcp_data_bytes;
      assert( sizeof(*vcpresp) == 8);
      assert(vcpresp->result_code == vcp_data_bytes[1]);  // validate the overlay
      aux_data->vcp_code = vcpresp->vcp_opcode;
      bool valid_response = true;

      if (vcpresp->vcp_opcode != requested_vcp_code){
         // if (debug) printf("(%s) Unexpected VCP opcode 0x%02x, should be 0x%02x\n",
         //                   __func__, vcpresp->vcp_opcode, requested_vcp_code);
         DDCMSG("Unexpected VCP opcode 0x%02x, should be 0x%02x",
                vcpresp->vcp_opcode, requested_vcp_code);
         result = COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
      }

      else if (vcpresp->result_code != 0) {
         if (vcpresp->result_code == 0x01) {
            // DBGMSF(debug, "Unsupported VCP Code");
            DDCMSG("Unsupported VCP Code: 0x%02x", vcpresp->vcp_opcode);
            aux_data->valid_response = true;
         }
         else {
            // DBGMSF(debug, "Unexpected result code: 0x%02x", vcpresp->result_code);
            DDCMSG("Unexpected result code: 0x%02x\n", vcpresp->result_code);
            result = COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
         }
      }

      else {
         int max_val = (vcpresp->max_val_hi_byte << 8) | vcpresp->max_val_lo_byte;
         int cur_val = (vcpresp->cur_val_hi_byte << 8) | vcpresp->cur_val_lo_byte;

         // if (debug) {
         //    printf("(%s) vcp_opcode = 0x%02x, vcp_type_code=0x%02x, max_val=%d (0x%04x), cur_val=%d (0x%04x)\n",
         //               __func__, vcpresp->vcp_opcode, vcpresp->vcp_typecode, max_val, max_val, cur_val, cur_val);
         //    DBGMSG("valid_response=%d", valid_response);
         // }
         TRCMSG("vcp_opcode = 0x%02x, vcp_type_code=0x%02x, max_val=%d (0x%04x), cur_val=%d (0x%04x)",
                vcpresp->vcp_opcode, vcpresp->vcp_typecode, max_val, max_val, cur_val, cur_val);
         TRCMSG("valid_response=%d", __func__, valid_response);

         aux_data->valid_response   = true;
         aux_data->supported_opcode = true;
         aux_data->max_value        = max_val;   // valid only for continuous features
         aux_data->cur_value        = cur_val;   // valid only for continuous features
         // for new way
         aux_data->mh = vcpresp->max_val_hi_byte;
         aux_data->ml = vcpresp->max_val_lo_byte;
         aux_data->sh = vcpresp->cur_val_hi_byte;
         aux_data->sl = vcpresp->cur_val_lo_byte;
      }
   }

   DBGMSF(debug, "Returning %s", psc_desc(result));
   return result;
}



void report_interpreted_nontable_vcp_response(
        Parsed_Nontable_Vcp_Response * interpreted, int depth)
{
   rpt_vstring(depth,"VCP code:         0x%02x", interpreted->vcp_code);
   rpt_vstring(depth,"valid_response:   %d",     interpreted->valid_response);
   rpt_vstring(depth,"supported_opcode: %d",     interpreted->supported_opcode);
   rpt_vstring(depth,"max_value:        %d",     interpreted->max_value);
   rpt_vstring(depth,"cur_value:        %d",     interpreted->cur_value);
   rpt_vstring(depth,"mh:               0x%02x", interpreted->mh);
   rpt_vstring(depth,"ml:               0x%02x", interpreted->ml);
   rpt_vstring(depth,"sh:               0x%02x", interpreted->sh);
   rpt_vstring(depth,"sl:               0x%02x", interpreted->sl);
}


void   report_parsed_vcp_response(Parsed_Vcp_Response * response, int depth) {
   rpt_vstring(depth, "Parsed_Vcp_Reponse at %p:", response);

   rpt_vstring(depth, "response_type:   %d",   response->response_type);
   if (response->response_type == DDCA_NON_TABLE_VCP_VALUE) {
      rpt_vstring(depth, "non_table_response at %p:", response->non_table_response);
      report_interpreted_nontable_vcp_response(response->non_table_response, depth+1);
   }
   else {
      rpt_vstring(depth, "table_response at %p", response->table_response);
   }
}




//
// Response packets 
//

/** Creates a #DDC_Packet for a DDC response.
 *
 *  \param i2c_response_bytes          pointer to raw packet bytes
 *  \param response_bytes_buffer_size  size of buffer pointed to by **i2c_response_bytes**
 *  \param expected_type               expected packet type
 *  \param expected_subtype            depends on expected_type
 *  \param tag                         debug string (may be NULL)
 *  \param packet_ptr_addr             where to return pointer to newly allocated #DDC_Packet
 *
 *  \retval 0
 *  \return as from #create_ddc_response_packet()
 *  \retval DDCRC_INVALID_DATA may be set by function that fills in aux_data struct
 *
 *  The pointer returned at packet_ptr_addr is non-null iff the status code is 0.
 *
 *  The contents of **expected_subtype** depends on the value of **expected_type**.
 *  For DDC_PACKET_TYPE_QUERY_VCP_RESPONSE it is the VCP feature code.
 */
Status_DDC create_ddc_typed_response_packet(
      Byte*           i2c_response_bytes,
      int             response_bytes_buffer_size,
      DDC_Packet_Type expected_type,
      Byte            expected_subtype,
      const char*     tag,
      DDC_Packet**    packet_ptr_addr)
{
   bool debug = false;
   if (debug) {
      char * hs =  hexstring(i2c_response_bytes,20);
      DBGMSF(debug, "Starting. i2c_response_bytes=%s", hs );
      free(hs);
   }

   // DBGMSG("before create_ddc_response_packet(), *packet_ptr_addr=%p", *packet_ptr_addr);
   // n. may return DDC_NULL_RESPONSE??   (old note)
   Status_DDC rc = create_ddc_response_packet(
               i2c_response_bytes,
               response_bytes_buffer_size,
               expected_type,
               tag,
               packet_ptr_addr);
   DBGMSF(debug, "Create_ddc_response_packet() returned %s, *packet_ptr_addr=%p",
                               __func__, psc_desc(rc), *packet_ptr_addr);
   if (rc == 0) {
      DDC_Packet * packet = *packet_ptr_addr;
      switch (expected_type) {

      case DDC_PACKET_TYPE_CAPABILITIES_RESPONSE:
      case DDC_PACKET_TYPE_TABLE_READ_RESPONSE:
         {
            Interpreted_Multi_Part_Read_Fragment * aux_data = calloc(1, sizeof(Interpreted_Multi_Part_Read_Fragment));
            packet->parsed.multi_part_read_fragment = aux_data;
            rc = interpret_multi_part_read_response(
                   expected_type,
                   get_data_start(packet),
                   get_data_len(packet),
                   aux_data);
         }
         break;

      case DDC_PACKET_TYPE_QUERY_VCP_RESPONSE:
         {
            Parsed_Nontable_Vcp_Response * aux_data = calloc(1, sizeof(Parsed_Nontable_Vcp_Response));
            packet->parsed.nontable_response = aux_data;
            rc = interpret_vcp_feature_response_std(
                    get_data_start(packet),
                    get_data_len(packet),
                    expected_subtype,
                    aux_data);
         }
         break;

      default:
         // what to do?  for now just bail
         TERMINATE_EXECUTION_ON_ERROR("Unhandled case. expected_type=%d", expected_type);
         break;
      }
   }

   if (rc != DDCRC_OK && *packet_ptr_addr) {
      free_ddc_packet(*packet_ptr_addr);
      *packet_ptr_addr = NULL;
   }

   DBGMSF(debug, "Returning %s, *packet_ptr=%p", ddcrc_desc(rc), *packet_ptr_addr);
   if ( debug && rc >= 0)
      dump_packet(*packet_ptr_addr);

   assert( (rc == 0 && *packet_ptr_addr) || (rc != 0 && !*packet_ptr_addr));
   return rc;
}


Status_DDC create_ddc_multi_part_read_response_packet(
                     Byte           response_type,
                     Byte *         i2c_response_bytes,
                     int            response_bytes_buffer_size,
                     const char *   tag,
                     DDC_Packet **  packet_ptr) {
   DDC_Packet * packet = NULL;
   Status_DDC rc = create_ddc_response_packet(i2c_response_bytes,
                                                     response_bytes_buffer_size,
                                                     DDC_PACKET_TYPE_TABLE_READ_RESPONSE,
                                                     tag,
                                                     &packet);
   if (rc != 0) {
      // DBGMSG("create_ddc_response_packet() returned %s, packet=%p", ddcrc_description(rc), packet);
      TRCMSG("create_ddc_response_packet() returned %s, packet=%p", ddcrc_desc(rc), packet);
   }
   if (rc == 0) {
      // dump_packet(packet);
      int min_data_len = 3;
      int max_data_len = 35;
      int data_len = get_data_len(packet);
      if (data_len < min_data_len || data_len > max_data_len) {
         DDCMSG("Invalid data fragment_length_wo_null: %d", data_len);
         if (IS_REPORTING_DDC())
            dump_packet(packet);
         rc = COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
      }
      else {
#ifdef OLD
         void * aux_data = calloc(1, sizeof(Interpreted_Multi_Part_Read_Fragment));
         packet->aux_data = aux_data;

         rc = interpret_multi_part_read_response(
                 response_type,
                 get_data_start(packet),
                 get_data_len(packet),
                 (Interpreted_Multi_Part_Read_Fragment *) aux_data);
#endif
         Interpreted_Multi_Part_Read_Fragment * aux_data = calloc(1, sizeof(Interpreted_Multi_Part_Read_Fragment));
         packet->parsed.multi_part_read_fragment = aux_data;

         rc = interpret_multi_part_read_response(
                 response_type,
                 get_data_start(packet),
                 get_data_len(packet),
                 aux_data);
      }
   }
   if (rc != 0 && packet) {
      free_ddc_packet(packet);
   }
   if (rc == 0)
      *packet_ptr = packet;
   return rc;
}


// VCP Feature response

// 4/2017: used only in ddc_vcp_tests.c:
Status_DDC
create_ddc_getvcp_response_packet(
       Byte *         i2c_response_bytes,
       int            response_bytes_buffer_size,
       Byte           expected_vcp_opcode,
       const char *   tag,
       DDC_Packet **  packet_ptr)
{
   DDC_Packet * packet = NULL;
   Status_DDC rc = create_ddc_response_packet(
               i2c_response_bytes,
               response_bytes_buffer_size,
               DDC_PACKET_TYPE_QUERY_VCP_RESPONSE,
               tag,
               &packet);
   if (rc != 0) {
      // DBGMSG("create_ddc_response_packet() returned %s, packet=%p", ddcrc_description(rc), packet);
      TRCMSG("create_ddc_response_packet() returned %s, packet=%p", ddcrc_desc(rc), packet);
   }
   if (rc == 0) {
      // dump_packet(packet);
      int data_len = get_data_len(packet);
      if (data_len != 8) {
         // DBGMSG("Invalid data length: %d, should be 8", data_len);
         // dump_packet(packet);
         DDCMSG("Invalid data length: %d, should be 8", data_len);
         if ( IS_REPORTING_DDC() )
            dump_packet(packet);
         rc = COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
      }
      else {
#ifdef OLD
         void * aux_data = calloc(1, sizeof(Parsed_Nontable_Vcp_Response));
         packet->aux_data = aux_data;

         rc =  interpret_vcp_feature_response_std(
                  get_data_start(packet),
                  get_data_len(packet),
                  expected_vcp_opcode,
                  (Parsed_Nontable_Vcp_Response *) aux_data);
#endif
         Parsed_Nontable_Vcp_Response * aux_data = calloc(1, sizeof(Parsed_Nontable_Vcp_Response));
         packet->parsed.nontable_response = aux_data;

         rc =  interpret_vcp_feature_response_std(
                  get_data_start(packet),
                  get_data_len(packet),
                  expected_vcp_opcode,
                  aux_data);
      }
   }
   if (rc != 0 && packet) {
      free_ddc_packet(packet);
   }
   if (rc == 0)
      *packet_ptr = packet;
   return rc;
}


//
// Operations on response packets
// 


// VCP Feature Code

/** Extracts the interpretation of a non-table VCP response from a #DDC_Packet.
 *
 *  This is the aux_data field of #DDC_Packet
 *
 * \param   packet  pointer to digested packet (not raw bytes)
 * \param   make_copy  if true, make a copy of the aux_data field,\n
 *                     if false, just return a pointer to it
 * \param   interpreted_ptr  where to return newly allocated #Parsed_Nontable_Vcp_Response
 * \retval  0    success
 * \retval  DDCRC_RESPONSE_TYPE  not a VCP response packet
 *
 * The value pointed to by **interpreted_ptr** is non-null iff the returned status code is 0.
 */
Status_DDC
get_interpreted_vcp_code(
       DDC_Packet *            packet,
       bool                    make_copy,
       Parsed_Nontable_Vcp_Response ** interpreted_ptr)
{
   bool debug = false;
   DBGMSF(debug, "Starting");

   Status_DDC rc = DDCRC_OK;
   if (packet->type != DDC_PACKET_TYPE_QUERY_VCP_RESPONSE) {
      COUNT_STATUS_CODE(DDCRC_RESPONSE_TYPE);
      rc = DDCRC_RESPONSE_TYPE;
      *interpreted_ptr = NULL;
   }
   else {
      if (make_copy) {
         Parsed_Nontable_Vcp_Response * copy =
            malloc(sizeof(Parsed_Nontable_Vcp_Response));
#ifdef OLD
         memcpy(copy, packet->aux_data, sizeof(Parsed_Nontable_Vcp_Response));
#endif
         memcpy(copy, packet->parsed.nontable_response, sizeof(Parsed_Nontable_Vcp_Response));
         *interpreted_ptr = copy;
      }
      else {
#ifdef OLD
         *interpreted_ptr = packet->aux_data;
#endif
         *interpreted_ptr = packet->parsed.nontable_response;
      }
   }
   DBGMSF(debug, "Returning %d: %s\n", rc, psc_desc(rc) );
   assert( (rc == 0 && *interpreted_ptr) || (rc && !*interpreted_ptr));
   return rc;
}


// 12/23/2015: not currently used
Status_DDC get_vcp_cur_value(DDC_Packet * packet, int * value_ptr) {
   Parsed_Nontable_Vcp_Response * aux_ptr;
   Status_DDC rc = get_interpreted_vcp_code(packet, false, &aux_ptr);
   if (rc == 0) {
      *value_ptr = aux_ptr->cur_value;
   }
   return rc;
}
