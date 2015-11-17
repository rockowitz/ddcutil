/*  ddc_packets.c
 *
 *  Created on: Jun 10, 2014
 *      Author: rock
 *
 *  Functions for creating DDC packets and interpreting DDC response packets.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <util/debug_util.h>
#include <util/string_util.h>

#include <base/ddc_errno.h>
#include <base/common.h>
#include <base/msg_control.h>
#include <base/status_code_mgt.h>
#include <base/util.h>

#include <base/ddc_packets.h>


// forward reference:
int interpret_vcp_feature_response(Byte * vcp_data_bytes,
                                   int bytect,
                                   Byte requested_vcp_code,
                                   Interpreted_Vcp_Code * aux_data,
                                   bool debug);

//
//  Initialization
//

void init_ddc_packets() {

}


//
// Trace control
//

// TraceControl ddc_packets_trace_level = NEVER;

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
   // printf("(%s) pb=%p, pb0=%p, *pb=0x%02x *pb0=0x%02x \n", __func__, pb, pb0, *pb, *pb0 );
   bool result = (*pb == *pb0);
   // printf("(%s) Returning: %d  \n", __func__, result );
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
   // printf("(%s) bytes=%p, len=%d, altmode=%d\n", __func__, bytes, len, altmode);
   // largest packet is capabilities fragment, which can have up to 32 bytes of text,
   // plus 4 bytes of offset data.  Adding this to the dest, src, and len bytes is 39
   assert(len <= MAX_DDC_PACKET_WO_CHECKSUM);
   Byte workbuf[MAX_DDC_PACKET_WO_CHECKSUM];
   Byte * bytes2 = bytes;
   Byte result;

   if (altmode) {
      // bytes2 = (unsigned char *) call_malloc(len,"ddc_checksum");
      bytes2 = workbuf;
      memcpy(bytes2, bytes, len);
      bytes2[0] = 0x50;
      // bytes2[1] = 0x50;   // alt
   }
   result = xor_bytes(bytes2, len);

   char * hs = hexstring(bytes2, len);
   // printf("(%s) checksum for %s is 0x%02x\n", __func__, hs, result);
   free(hs);

   // if (altmode)
   //    call_free(bytes2, "ddc_checksum");
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
   // printf("(%s) data_size = %d\n", __func__, data_size);
   if (data_size > MAX_DDCCI_PACKET_SIZE) {    // correct constant?
      // printf("(%s) Invalid data_size = %d\n", __func__, data_size);
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

   // printf("(%s) Returning: %d\n", __func__, result);
   return result;
}


//
//  Packet general functions 
//

Byte * get_packet_start(DDC_Packet * packet) {
   Byte * result = NULL;
   if (packet)
      result = packet->buf->bytes;
   return result;
}


int get_packet_len(DDC_Packet * packet) {
   return (packet) ? packet->buf->len : 0;
}


int get_data_len(DDC_Packet * packet) {
   return (packet) ? packet->buf->len - 4 : 0;
}


Byte * get_data_start(DDC_Packet * packet) {
   return (packet) ? packet->buf->bytes+3 : NULL;
}


int get_packet_max_size(DDC_Packet * packet) {
   return packet->buf->buffer_size;
}


void dump_packet(DDC_Packet * packet) {
   printf("DDC_Packet dump.  Addr: %p, Type: 0x%02x, Tag: |%s|, buf: %p, aux_data: %p\n",
          packet, packet->type, packet->tag, packet->buf, packet->aux_data);
   // hex_dump(packet->buf->bytes, packet->buf->len);
   buffer_dump(packet->buf);
   // TODO show interpreted aux_data
   if (packet->aux_data)
      report_interpreted_aux_data(packet->type, packet->aux_data);
}

void report_interpreted_aux_data(Byte response_type, void * aux_data) {
   assert(aux_data);

   printf("Interpreting aux data at %p for response type: 0x%02x\n", aux_data, response_type);
   switch (response_type) {
   case (DDC_PACKET_TYPE_CAPABILITIES_RESPONSE):
   case (DDC_PACKET_TYPE_TABLE_READ_RESPONSE):
      report_interpreted_multi_read_fragment(aux_data);
      break;
   case (DDC_PACKET_TYPE_QUERY_VCP_RESPONSE):
      report_interpreted_vcp_code(aux_data);
      break;

   default:
      printf("(%s) Don't know how to interpret aux data for response type: 0x%02x\n", __func__, response_type);
   }


}



bool isNullPacket(DDC_Packet * packet) {
   return  (get_data_len(packet) == 0);
}


void free_ddc_packet(DDC_Packet * packet) {
   bool debug = false;
   bool tf = IS_TRACING();
   if (debug)
      tf = true;
   TRCMSGTF(tf, "packet=%p", packet);

   // dump_packet(packet);

   if (packet) {
      if (packet->aux_data) {
         if (debug)
            printf("(%s) freeing packet->aux_data=%p\n", __func__, packet->aux_data);
         call_free(packet->aux_data, "free_ddc_packet:aux_data");
      }

      if (debug)
         printf("(%s) calling free_buffer() for packet->buf=%p\n", __func__, packet->buf);
      buffer_free(packet->buf, "free DDC packet");

      if (debug)
         printf("(%s) freeing packet=%p\n", __func__, packet);
      call_free(packet, "free_ddc_packet:packet");
   }
   TRCMSGTF(tf, "Done" );
}


/* Base function for creating any DDC packet
 *
 * Arguments:
 *    max_size  size of buffer allocated for packet bytes
 *    tag       debug string
 *
 * Returns:
 *    pointer to created DDC_Packet
 */
DDC_Packet * create_empty_ddc_packet(int max_size, const char * tag) {
   bool debug = false;
   DBGMSGF(debug, "Starting. max_size=%d, tag=%s", max_size, (tag) ? tag : "(nil)");

   DDC_Packet * packet = call_malloc(sizeof(DDC_Packet), "create_empty_ddc_packet:packet");
   packet->buf = buffer_new(max_size, "empty DDC packet");
   if (tag) {
      strncpy(packet->tag, tag, MAX_DDC_TAG);
      packet->tag[MAX_DDC_TAG] = '\0';    // in case we maxed out
   }
   else
      packet->tag[0] = '\0';
   // printf("(%s) packet->tag=%s\n", __func__, packet->tag);
   packet->type = DDC_PACKET_TYPE_NONE;
   packet->aux_data = NULL;

   DBGMSGF(debug, "Done. Returning %p, packet->tag=%p", packet, packet->tag);
   if (debug)
      dump_packet(packet);

   return packet;
}


//
// Request Packets
//

/* Create a generic DDC request packet
 *
 * Arguments:
 *    data_bytes data bytes of packet
 *    bytect     number of data bytes
 *    tag        debug string
 *
 * Returns:
 *    pointer to created packet
 */
DDC_Packet * create_ddc_base_request_packet(
                Byte *       data_bytes,
                int          data_bytect,
                const char * tag) {
   char * hs =  hexstring(data_bytes, data_bytect);
   TRCMSG("Starting.  bytes=%s, tag=%s", hs, tag);
   free(hs);

   assert( data_bytect <= 32 );

   DDC_Packet * packet = create_empty_ddc_packet(3+data_bytect+1, tag);
   buffer_set_byte( packet->buf, 0, 0x6e);
   buffer_set_byte( packet->buf, 1, 0x51);
   buffer_set_byte( packet->buf, 2, data_bytect | 0x80);
   buffer_set_bytes(packet->buf, 3, data_bytes, data_bytect);
   int packet_size_wo_checksum = 3 + data_bytect;
   Byte checksum = ddc_checksum(packet->buf->bytes, packet_size_wo_checksum, false);
   buffer_set_byte(packet->buf, packet_size_wo_checksum, checksum);
   buffer_set_length(packet->buf, 3 + data_bytect + 1);
   if (data_bytect > 0)
      packet->type = data_bytes[0];
   else
      packet->type = 0x00;
   // dump_buffer(packet->buf);

   TRCMSG("Done. packet=%p", packet);
   return packet;
}


#ifdef OLD
/* Creates a DDC capabilities request packet
 *
 * Arguments:
 *    offset  offset value
 *    tag     debug string
 *
 * Returns:
 *    pointer to created capabilities request packet
 */
DDC_Packet * create_ddc_capabilities_request_packet(int offset, const char * tag) {
   DDC_Packet * packet_ptr = NULL;

   Byte ofs_hi_byte = (offset >> 16) & 0xff;
   Byte ofs_lo_byte = offset & 0xff;

   Byte data_bytes[] = { DDC_PACKET_TYPE_CAPABILITIES_REQUEST ,
                         ofs_hi_byte,
                         ofs_lo_byte
                       };
   packet_ptr = create_ddc_base_request_packet(data_bytes, 3, tag);

   // printf("(%s) Done. packet_ptr=%p\n", __func__, packet_ptr);
   // dump_packet(packet_ptr);
   return packet_ptr;
}
#endif


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
                const char * tag) {
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

   // printf("(%s) Done. packet_ptr=%p\n", __func__, packet_ptr);
   // dump_packet(packet_ptr);
   return packet_ptr;
}


#ifdef OLD
/* Updates the offset in a DDC capabilities request packet
 *
 * Arguments:
 *    packet  address of packet
 *    offset  new offset value
 */
void update_ddc_capabilities_request_packet_offset(DDC_Packet * packet, int offset) {
   assert(packet->type == DDC_PACKET_TYPE_CAPABILITIES_REQUEST);

   // update offset
   Byte * data_bytes = get_data_start(packet);
   Byte ofs_hi_byte = (offset >> 8) & 0xff;
   // ofs_hi_byte = 0x00;                             // *** TEMP *** INSERT BUG
   Byte ofs_lo_byte = offset & 0xff;
   data_bytes[1] = ofs_hi_byte;
   data_bytes[2] = ofs_lo_byte;
   // printf("(%s) offset=%d, ofs_hi_byte=0x%02x, ofs_lo_byte=0x%02x\n", __func__, offset, ofs_hi_byte, ofs_lo_byte );

   // update checksum
   Byte * bytes = get_packet_start(packet);
   int packet_size_wo_checksum = get_packet_len(packet)-1;
   bytes[packet_size_wo_checksum] = ddc_checksum(bytes, packet_size_wo_checksum, false);

   // printf("(%s) Done.\n", __func__);
   // dump_packet(packet);
}
#endif


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
   // printf("(%s) offset=%d, ofs_hi_byte=0x%02x, ofs_lo_byte=0x%02x\n", __func__, new_offset, ofs_hi_byte, ofs_lo_byte );

   // update checksum
   Byte * bytes = get_packet_start(packet);
   int packet_size_wo_checksum = get_packet_len(packet)-1;
   bytes[packet_size_wo_checksum] = ddc_checksum(bytes, packet_size_wo_checksum, false);

   // printf("(%s) Done.\n", __func__);
   // dump_packet(packet);
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
DDC_Packet * create_ddc_getvcp_request_packet(Byte vcp_code, const char * tag) {

   Byte data_bytes[] = { 0x01,     // Command: get VCP Feature
                         vcp_code  // VCP opcode
                       };
   DDC_Packet * pkt = create_ddc_base_request_packet(data_bytes, 2, tag);

   // printf("(%s) Done. rc=%d, packet_ptr%p, *packet_ptr=%p\n", __func__, rc, packet_ptr, *packet_ptr);
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
DDC_Packet * create_ddc_setvcp_request_packet(Byte vcp_code, int new_value, const char * tag) {
   Byte data_bytes[] = { 0x03,   // Command: get VCP Feature
                         vcp_code,  // VCP opcode
                         (new_value >> 8) & 0xff,
                         new_value & 0xff
                       };
   DDC_Packet * pkt = create_ddc_base_request_packet(data_bytes, 4, tag);

   // printf("(%s) Done. rc=%d, packet_ptr%p, *packet_ptr=%p\n", __func__, rc, packet_ptr, *packet_ptr);
   return pkt;
}


//
// Response Packets
//

Global_Status_DDC create_ddc_base_response_packet(
                     Byte *        i2c_response_bytes,
                     int           response_bytes_buffer_size,
                     const char *  tag,
                     DDC_Packet ** packet_ptr_addr)
{
   // bool debug = adjust_debug_level(true, ddc_packets_trace_level);
   // if (debug)
   //    printf("(%s) Starting. i2c_response_bytes=%s\n", __func__, hexstring(i2c_response_bytes,20));
   char * hs = hexstring(i2c_response_bytes,20);
   TRCMSG("Starting. i2c_response_bytes=%s", hs );
   free(hs);

   int result = DDCRC_OK;
   DDC_Packet * packet = NULL;
   if (i2c_response_bytes[0] != 0x6e ) {
      // printf("(%s) Unexpected source address 0x%02x, should be 0x6e\n", __func__, i2c_response_bytes[0]);
      DDCMSG("Unexpected source address 0x%02x, should be 0x6e", i2c_response_bytes[0]);
      result = DDCRC_RESPONSE_ENVELOPE;
   }
   else {
      int data_ct = i2c_response_bytes[1] & 0x7f;
      // printf("(%s) data_ct=%d\n", __func__, data_ct);
      if (data_ct > MAX_DDC_DATA_SIZE) {
         if ( is_double_byte(&i2c_response_bytes[1])) {
            result = DDCRC_DOUBLE_BYTE;
            // if (showRecoverableErrors || debug)
            //    printf("(%s) Double byte in packet.\n", __func__);
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
         // printf("(%s) create_empty_ddc_packet() returned %p\n", __func__, packet);
         if (data_ct > 0)
            packet->type = i2c_response_bytes[2];
         Byte * packet_bytes = packet->buf->bytes;
         // printf("(%s) packet_bytes=%p\n", __func__, packet_bytes);
         // packet_bytes[0] = 0x6f;    // implicit, would be 0x50 on access bus
         // packet_bytes[1] = 0x6e;       // i2c_response_bytes[0[
         // memcpy(packet_bytes+2, i2c_response_bytes+1, 1 + data_ct + 1);
         // packet->buf->len = 3 + data_ct + 1;
         buffer_set_byte(  packet->buf, 0, 0x6f);     // implicit, would be 0x50 on access bus
         buffer_set_byte(  packet->buf, 1, 0x6e);     // i2c_response_bytes[0]
         buffer_set_bytes( packet->buf, 2, i2c_response_bytes+1, 1 + data_ct + 1);
         buffer_set_length(packet->buf, 3 + data_ct + 1);
         Byte calculated_checksum = ddc_checksum(packet_bytes, 3 + data_ct, true);   // replacing right byte?
         Byte actual_checksum = packet_bytes[3+data_ct];
         if (calculated_checksum != actual_checksum) {
            // if (showRecoverableErrors || debug)
            //    printf("(%s) Actual checksum 0x%02x, expected 0x%02x\n",
            //           __func__, actual_checksum, calculated_checksum);
            DDCMSG("Actual checksum 0x%02x, expected 0x%02x",
                   actual_checksum, calculated_checksum);
            // printf("(%s) !!! SUPPRESSING CHECKSUM ERROR\n", __func__);
            result = DDCRC_CHECKSUM;
            // printf("(%s) Freeing packet=%p\n", __func__, packet);
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
   // if (debug || (result != 0 && showRecoverableErrors) ) {
   //    printf("(%s) returning %s, *packet_ptr_addr=%p\n",
   //           __func__, ddcrc_description(result), *packet_ptr_addr);
   // }
   TRCMSG("returning %s, *packet_ptr_addr=%p\n",
          ddcrc_description(result), *packet_ptr_addr);

   assert( (result==DDCRC_OK && *packet_ptr_addr) || (result != DDCRC_OK && !*packet_ptr_addr));
   return result;
}


Global_Status_DDC create_ddc_response_packet(
       Byte *         i2c_response_bytes,
       int            response_bytes_buffer_size,
       Byte           expected_type,
       const char *   tag,
       DDC_Packet **  packet_ptr_addr) {
   // bool debug = adjust_debug_level(true, ddc_packets_trace_level);
   // if (debug)
   //    printf("(%s) Starting. i2c_response_bytes=%s\n", __func__, hexstring(i2c_response_bytes,20));
   char * hs = hexstring(i2c_response_bytes,20);
   TRCMSG("Starting. i2c_response_bytes=%s", hs);
   free(hs);
   int result = create_ddc_base_response_packet(i2c_response_bytes, response_bytes_buffer_size, tag,  packet_ptr_addr);
   // printf("(%s) create_ddc_base_response_packet() returned %d, *packet_ptr_addr=%p\n", __func__, result, *packet_ptr_addr);
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
      //    printf("(%s) failure, freeing response packet at %p\n", __func__, *packet_ptr_addr);
      TRCMSG("failure, freeing response packet at %p", *packet_ptr_addr);
      // does this cause the free(readbuf) failure in try_read?
      free_ddc_packet(*packet_ptr_addr);
      *packet_ptr_addr = 0;
   }

   // if (debug)
   //    printf("(%s) returning %s, *packet_ptr_addr=%p\n", __func__, ddcrc_description(result), *packet_ptr_addr);
   TRCMSG("returning %s, *packet_ptr_addr=%p", ddcrc_description(result), *packet_ptr_addr);
   assert( (result==DDCRC_OK && *packet_ptr_addr) || (result != DDCRC_OK && !*packet_ptr_addr));
   return result;
}



//
// Packet data parsers
//


// Capabilities response data 

#ifdef OLD
Global_Status_DDC interpret_capabilities_response(
       Byte * data_bytes,
       int    bytect,
       Interpreted_Capabilities_Fragment * aux_data,
       bool debug)
{
   // debug = true;
   int result = DDCRC_OK;

   // not needed, already checked
   if (bytect < 3 || bytect > 35) {
      // if (debug)
      // printf("(%s) Invalid response data length: %d\n", __func__, bytect);
      DDCMSG("(DDCMSG) Invalid response data length: %d", bytect);
      result = COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
   }
   else {
      assert( data_bytes[0] == 0xe3);             // Capabilities reply opcode
      Byte   offset_hi_byte = data_bytes[1];
      Byte   offset_lo_byte = data_bytes[2];
      int    text_length = bytect-3;    // max 32
      char * text_start = (char *) data_bytes+3;

      // printf("(%s) offset_hi_byte = 0x%02x, offset_lo_byte = 0x%02x\n", __func__, offset_hi_byte, offset_lo_byte );
      aux_data->fragment_offset = offset_hi_byte << 8 | offset_lo_byte;
      aux_data->fragment_length_wo_null = text_length;
      assert(text_length <= MAX_DDC_CAPABILITIES_FRAGMENT_SIZE);
      memcpy(aux_data->text, text_start, text_length);
      aux_data->text[text_length] = '\0';
   }
   // printf("(%s) returning %s\n", __func__, ddcrc_description(result));
   return result;
}
#endif

void report_interpreted_capabilities(Interpreted_Capabilities_Fragment * interpreted) {
   printf("Capabilities response contents:\n");
   printf("   offset:          %d\n", interpreted->fragment_offset);
   printf("   fragment length: %d\n", interpreted->fragment_length_wo_null);
   printf("   text:            |%.*s|\n", interpreted->fragment_length_wo_null, interpreted->text);
}



Global_Status_DDC interpret_multi_part_read_response(
       Byte   response_type,
       Byte * data_bytes,
       int    bytect,
       Interpreted_Multi_Part_Read_Fragment * aux_data,    //   Interpreted_Capabilities_Fragment * aux_data,
       bool debug)
{
   debug = false;
   int result = DDCRC_OK;

   // not needed, already checked
   if (bytect < 3 || bytect > 35) {
      // if (debug)
      // printf("(%s) Invalid response data length: %d\n", __func__, bytect);
      DDCMSG("(DDCMSG) Invalid response data length: %d", bytect);
      result = COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
   }
   else {
      assert( data_bytes[0] == response_type);             // table read reply opcode    // CHANGED
      Byte   offset_hi_byte = data_bytes[1];
      Byte   offset_lo_byte = data_bytes[2];
      int    read_data_length = bytect-3;    // max 32  // CHANGED
      Byte * read_data_start = data_bytes+3;    // CHANGED

      // printf("(%s) offset_hi_byte = 0x%02x, offset_lo_byte = 0x%02x\n", __func__, offset_hi_byte, offset_lo_byte );
      aux_data->fragment_type = response_type;    // set in caller?  would make response_type parm unnecessary
      aux_data->fragment_offset = offset_hi_byte << 8 | offset_lo_byte;
      aux_data->fragment_length = read_data_length;      // changed
      assert(read_data_length <= MAX_DDC_CAPABILITIES_FRAGMENT_SIZE);   // ???
      memcpy(aux_data->bytes, read_data_start, read_data_length);    // CHANGED
      // aux_data->text[text_length] = '\0';     // CHANGED
   }
   if (debug)
      printf("(%s) returning %s\n", __func__, ddcrc_description(result));
   return result;
}



void report_interpreted_multi_read_fragment(Interpreted_Multi_Part_Read_Fragment * interpreted) {
   printf("Multi-read response contents:\n");
   printf("   frament type:    0x%02x\n", interpreted->fragment_type);
   printf("   offset:          %d\n", interpreted->fragment_offset);
   printf("   fragment length: %d\n", interpreted->fragment_length);
   printf("   data addr:       %p\n", interpreted->bytes);
   if (interpreted->fragment_type == DDC_PACKET_TYPE_CAPABILITIES_RESPONSE)
   printf("   text:            |%.*s|\n", interpreted->fragment_length, interpreted->bytes);
   else {
      char * hs = hexstring(interpreted->bytes, interpreted->fragment_length);
      printf("   data:            0x%s\n", hs);
      free(hs);
   }
}


//  VCP feature response data 

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


Global_Status_DDC interpret_vcp_feature_response_std(
       Byte*                 vcp_data_bytes,
       int                   bytect,
       Byte                  requested_vcp_code,
       Interpreted_Vcp_Code* aux_data,   // record in which interpreted feature response will be stored
       bool                  debug)
{
   // debug = false;
   // if (debug)
   //    printf("(%s) Starting\n", __func__);
   TRCMSG("Starting.");

   int result = DDCRC_OK;

                // To be used:
                // Vcp_Response  vcp_response;
                // Vcp_Response * vcp_response_ptr;

   // bool valid_response     = true;
   // bool supported_vcp_code = true;

   // set initial values for failure case
   aux_data->vcp_code         = 0x00;
   aux_data->valid_response   = false;
   aux_data->supported_opcode = false;
   aux_data->max_value        = 0;
   aux_data->cur_value        = 0;

   if (bytect != 8) {
      // if (debug)
      // printf("(%s) Invalid response data length: %d, should be 8\n", __func__, bytect);
      DDCMSG("(DDCMSG) Invalid response data length: %d, should be 8", bytect);
      result = COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
   }
   else {
#ifdef OLD
      // TODO: replace with union
      Byte result_code     = vcp_data_bytes[1];
      Byte vcp_opcode      = vcp_data_bytes[2];
      Byte vcp_type_code   = vcp_data_bytes[3];
      Byte max_val_hi_byte = vcp_data_bytes[4];
      Byte max_val_lo_byte = vcp_data_bytes[5];
      Byte cur_val_hi_byte = vcp_data_bytes[6];
      Byte cur_val_lo_byte = vcp_data_bytes[7];
#endif

      Vcp_Response * vcpresp = (Vcp_Response *) vcp_data_bytes;
      assert( sizeof(*vcpresp) == 8);
      assert(vcpresp->result_code == vcp_data_bytes[1]);
#ifdef OLD
      assert(vcpresp->result_code == result_code);
      assert(vcpresp->cur_val_lo_byte == cur_val_lo_byte);
#endif


      aux_data->vcp_code = vcpresp->vcp_opcode;

      bool valid_response = true;
      if (vcpresp->result_code != 0) {
         if (vcpresp->result_code == 0x01) {
            // if (debug)
            //    printf("(%s) Unsupported VCP Code\n", __func__);
            DDCMSG("Unsupported VCP Code", __func__);
            aux_data->valid_response = true;
         }
         else {
            // if (debug) printf("(%s) Unexpected result code: 0x%02x\n", __func__, vcpresp->result_code);
            DDCMSG("Unexpected result code: 0x%02x\n", vcpresp->result_code);
            result = COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
         }
      }
      else if (vcpresp->vcp_opcode != requested_vcp_code){
         // if (debug) printf("(%s) Unexpected VCP opcode 0x%02x, should be 0x%02x\n",
         //                   __func__, vcpresp->vcp_opcode, requested_vcp_code);
         DDCMSG("Unexpected VCP opcode 0x%02x, should be 0x%02x\n",
                vcpresp->vcp_opcode, requested_vcp_code);
         result = COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
      }
      else {
         int max_val = (vcpresp->max_val_hi_byte << 8) | vcpresp->max_val_lo_byte;
         int cur_val = (vcpresp->cur_val_hi_byte << 8) | vcpresp->cur_val_lo_byte;

         // if (debug) {
         //    printf("(%s) vcp_opcode = 0x%02x, vcp_type_code=0x%02x, max_val=%d (0x%04x), cur_val=%d (0x%04x)\n",
         //               __func__, vcpresp->vcp_opcode, vcpresp->vcp_typecode, max_val, max_val, cur_val, cur_val);
         //    printf("(%s) valid_response=%d\n", __func__, valid_response);
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

   // if (debug)
   //    printf("(%s) returning %s\n", __func__, ddcrc_description(result));
   TRCMSG("returning %s\n", __func__, ddcrc_description(result));
   return result;
}



Global_Status_DDC interpret_vcp_feature_response(Byte * vcp_data_bytes,
                                   int bytect,
                                   Byte requested_vcp_code,
                                   Interpreted_Vcp_Code * aux_data,
                                   bool debug)
{

#ifdef IN_PROGRESS
   debug = false;
   if (debug)
      printf("(%s) Starting\n", __func__);

   int result = DDCRC_OK;

                   // To be used:
                   // Vcp_Response  vcp_response;
                   // Vcp_Response * vcp_response_ptr;

      // bool valid_response     = true;
      // bool supported_vcp_code = true;

      // set initial values for failure case
      aux_data->vcp_code = 0x00;
      aux_data->valid_response = false;
      aux_data->supported_opcode = false;
      aux_data->max_value = 0;
      aux_data->cur_value = 0;

      if (bytect != 8) {
         // if (debug)
         printf("(%s) Invalid response data length: %d, should be 8\n", __func__, bytect);
         result = COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
      }
      else {
         // TODO: replace with union
         Byte result_code     = vcp_data_bytes[1];
         Byte vcp_opcode      = vcp_data_bytes[2];
         Byte vcp_type_code   = vcp_data_bytes[3];
         Byte max_val_hi_byte = vcp_data_bytes[4];
         Byte max_val_lo_byte = vcp_data_bytes[5];
         Byte cur_val_hi_byte = vcp_data_bytes[6];
         Byte cur_val_lo_byte = vcp_data_bytes[7];

         aux_data->vcp_code = vcp_opcode;

         bool valid_response = true;
         if (result_code != 0) {
            if (result_code == 0x01) {
               if (debug)
                  printf("(%s) Unsupported VCP Code\n", __func__);
               aux_data->valid_response = true;
            }
            else {
               if (debug) printf("(%s) Unexpected result code: 0x%02x\n", __func__, result_code);
               result = COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
            }
         }
         else if (vcp_opcode != requested_vcp_code){
            if (debug) printf("(%s) Unexpected VCP opcode 0x%02x, should be 0x%02x\n", __func__, vcp_opcode, requested_vcp_code);
            result = COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
         }
         else {
#endif

   int rc;

   rc = interpret_vcp_feature_response_std(vcp_data_bytes,
                                           bytect,
                                           requested_vcp_code,
                                           aux_data,
                                           debug);
   return rc;
}


void report_interpreted_vcp_code(Interpreted_Vcp_Code * interpreted) {
   printf("VCP code:         0x%02x\n", interpreted->vcp_code);
   printf("valid_response:   %d\n",     interpreted->valid_response);
   printf("supported_opcode: %d\n",     interpreted->supported_opcode);
   printf("max_value:        %d\n",     interpreted->max_value);
   printf("cur_value:        %d\n",     interpreted->cur_value);
   printf("mh:               0x%02x\n", interpreted->mh);
   printf("ml:               0x%02x\n", interpreted->ml);
   printf("sh:               0x%02x\n", interpreted->sh);
   printf("sl:               0x%02x\n", interpreted->sl);
}




//
// Response packets 
//

Global_Status_DDC create_ddc_typed_response_packet(
      Byte*        i2c_response_bytes,
      int          response_bytes_buffer_size,
      Byte         expected_type,
      Byte         expected_subtype,
      const char*        tag,
      DDC_Packet** packet_ptr_addr)
{
   // bool debug = adjust_debug_level(true, ddc_packets_trace_level);

   // if (debug)
   //    printf("(%s) Starting. i2c_response_bytes=%s\n", __func__, hexstring(i2c_response_bytes,20));
   char * hs =  hexstring(i2c_response_bytes,20);
   TRCMSG("Starting. i2c_response_bytes=%s", hs );
   free(hs);

   void * aux_data;

   // printf("(%s) before create_ddc_response_packet(), *packet_ptr_addr=%p\n", __func__, *packet_ptr_addr);
   // n. may return DDC_NULL_RESPONSE??   (old note)
   Global_Status_DDC rc = create_ddc_response_packet(
               i2c_response_bytes,
               response_bytes_buffer_size,
               expected_type,
               tag,
               packet_ptr_addr);
   // printf("(%s) create_ddc_response_packet() returned %d, *packet_ptr_addr=%p\n",
   //       __func__, rc, *packet_ptr_addr);
   if (rc == 0) {
      DDC_Packet * packet = *packet_ptr_addr;
      switch (expected_type) {
      case DDC_PACKET_TYPE_CAPABILITIES_RESPONSE:
//         aux_data = call_calloc(1, sizeof(Interpreted_Capabilities_Fragment),
//                                "create_ddc_capabilities_response_packet:aux_data");
//         packet->aux_data = aux_data;
//
//         rc = interpret_capabilities_response(
//                get_data_start(packet),
//                get_data_len(packet),
//                (Interpreted_Capabilities_Fragment *) aux_data,
//                true);
//         break;
      case DDC_PACKET_TYPE_TABLE_READ_RESPONSE:
         aux_data = call_calloc(1, sizeof(Interpreted_Multi_Part_Read_Fragment),
                                "create_ddc_typed_response_packet:table_read:aux_data");
         packet->aux_data = aux_data;

         rc = interpret_multi_part_read_response(
                expected_type,
                get_data_start(packet),
                get_data_len(packet),
                (Interpreted_Multi_Part_Read_Fragment *) aux_data,
                true);
         break;
      case DDC_PACKET_TYPE_QUERY_VCP_RESPONSE:
         aux_data = call_calloc(1, sizeof(Interpreted_Vcp_Code),
                                "create_ddc_getvcp_response_packet:aux_data");
         packet->aux_data = aux_data;

         rc = interpret_vcp_feature_response(get_data_start(packet),
                                            get_data_len(packet),
                                            expected_subtype,
                                            (Interpreted_Vcp_Code *) aux_data,
                                            true);
         break;

      default:
         // what to do?  for now just bail
         TERMINATE_EXECUTION_ON_ERROR("Unhandled case. expected_type=%d", expected_type);
         // printf("something\n");
         break;
      }
   }

   if (rc != DDCRC_OK && *packet_ptr_addr) {
      free_ddc_packet(*packet_ptr_addr);
   }

   // if (debug) {
   //    printf("(%s) returning %s, *packet_ptr=%p\n", __func__, ddcrc_description(rc), *packet_ptr_addr);
   //    if (rc >= 0)
   //       dump_packet(*packet_ptr_addr);
   // }
   TRCMSG("returning %s, *packet_ptr=%p", ddcrc_description(rc), *packet_ptr_addr);
   if ( IS_TRACING() ) {
      if (rc >= 0)
         dump_packet(*packet_ptr_addr);
   }
   return rc;
}

#ifdef OLD
Global_Status_DDC create_ddc_capabilities_response_packet(
                     Byte *         i2c_response_bytes,
                     int            response_bytes_buffer_size,
                     const char *   tag,
                     DDC_Packet **  packet_ptr) {
   DDC_Packet * packet = NULL;
   Global_Status_DDC rc = create_ddc_response_packet(i2c_response_bytes,
                                                     response_bytes_buffer_size,
                                                     DDC_PACKET_TYPE_CAPABILITIES_RESPONSE,
                                                     tag,
                                                     &packet);
   if (rc != 0) {
      // printf("(%s) create_ddc_response_packet() returned %s, packet=%p\n", __func__, ddcrc_description(rc), packet);
      TRCMSG("create_ddc_response_packet() returned %s, packet=%p", ddcrc_description(rc), packet);
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
         void * aux_data = call_calloc(1, sizeof(Interpreted_Capabilities_Fragment),
                                                 "create_ddc_capabilities_response_packet:aux_data");
         packet->aux_data = aux_data;

         rc = interpret_capabilities_response(
                 get_data_start(packet),
                 get_data_len(packet),
                 (Interpreted_Capabilities_Fragment *) aux_data,
                 true);
      }
   }
   if (rc != 0 && packet) {
      free_ddc_packet(packet);
   }
   if (rc == 0)
      *packet_ptr = packet;
   return rc;
}
#endif


Global_Status_DDC create_ddc_multi_part_read_response_packet(
                     Byte           response_type,
                     Byte *         i2c_response_bytes,
                     int            response_bytes_buffer_size,
                     const char *   tag,
                     DDC_Packet **  packet_ptr) {
   DDC_Packet * packet = NULL;
   Global_Status_DDC rc = create_ddc_response_packet(i2c_response_bytes,
                                                     response_bytes_buffer_size,
                                                     DDC_PACKET_TYPE_TABLE_READ_RESPONSE,
                                                     tag,
                                                     &packet);
   if (rc != 0) {
      // printf("(%s) create_ddc_response_packet() returned %s, packet=%p\n", __func__, ddcrc_description(rc), packet);
      TRCMSG("create_ddc_response_packet() returned %s, packet=%p", ddcrc_description(rc), packet);
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
         void * aux_data = call_calloc(1, sizeof(Interpreted_Capabilities_Fragment),
                                                 "create_ddc_capabilities_response_packet:aux_data");
         packet->aux_data = aux_data;

         rc = interpret_multi_part_read_response(
                 response_type,
                 get_data_start(packet),
                 get_data_len(packet),
                 (Interpreted_Multi_Part_Read_Fragment *) aux_data,
                 true);
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

Global_Status_DDC create_ddc_getvcp_response_packet(
       Byte *         i2c_response_bytes,
       int            response_bytes_buffer_size,
       Byte           expected_vcp_opcode,
       const char *   tag,
       DDC_Packet **  packet_ptr) {
   DDC_Packet * packet = NULL;
   Global_Status_DDC rc = create_ddc_response_packet(
               i2c_response_bytes,
               response_bytes_buffer_size,
               DDC_PACKET_TYPE_QUERY_VCP_RESPONSE,
               tag,
               &packet);
   if (rc != 0) {
      // printf("(%s) create_ddc_response_packet() returned %s, packet=%p\n", __func__, ddcrc_description(rc), packet);
      TRCMSG("create_ddc_response_packet() returned %s, packet=%p", ddcrc_description(rc), packet);
   }
   if (rc == 0) {
      // dump_packet(packet);
      int data_len = get_data_len(packet);
      if (data_len != 8) {
         // printf("(%s) Invalid data length: %d, should be 8\n", __func__, data_len);
         // dump_packet(packet);
         DDCMSG("Invalid data length: %d, should be 8", data_len);
         if ( IS_REPORTING_DDC() )
            dump_packet(packet);
         rc = COUNT_STATUS_CODE(DDCRC_INVALID_DATA);
      }
      else {
         void * aux_data = call_calloc(1, sizeof(Interpreted_Vcp_Code),
                                       "create_ddc_getvcp_response_packet:aux_data");
         packet->aux_data = aux_data;

         rc = interpret_vcp_feature_response(get_data_start(packet),
                                             get_data_len(packet),
                                             expected_vcp_opcode,
                                             (Interpreted_Vcp_Code *) aux_data,
                                             true);
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

// Capabilities 

#ifdef OLD
Global_Status_DDC get_interpreted_capabilities_fragment(
                     DDC_Packet *  packet,
                     bool          make_copy,
                     Interpreted_Capabilities_Fragment ** interpreted_ptr)
{
   // printf("(%s) Starting\n", __func__);
   Global_Status_DDC rc = DDCRC_OK;
   if (packet->type != DDC_PACKET_TYPE_CAPABILITIES_RESPONSE)
      rc = COUNT_STATUS_CODE(DDCRC_RESPONSE_TYPE);
   else {
      if (make_copy) {
         Interpreted_Capabilities_Fragment * copy =
            call_malloc(sizeof(Interpreted_Capabilities_Fragment),
                        "get_interpreted_capabilities_fragmente:make_copy");
         memcpy(copy, packet->aux_data, sizeof(Interpreted_Capabilities_Fragment));
         *interpreted_ptr = copy;
      }
      else {
         *interpreted_ptr = packet->aux_data;
      }
   }
   return rc;
}
#endif

#ifdef OLD
int get_capabilities_offset(DDC_Packet * packet, int * offset_ptr) {
   Interpreted_Capabilities_Fragment * aux_ptr;
   int rc = get_interpreted_capabilities_fragment(packet, false, &aux_ptr );
   if (rc == 0) {
      *offset_ptr = aux_ptr->fragment_offset;
   }

   return rc;
}
#endif

#ifdef UNUSED

int get_capabilities_fragment(DDC_Packet * packet, char ** fragment_ptr) {
   Interpreted_Capabilities_Fragment * aux_ptr;
   int rc = get_interpreted_capabilities_fragment(packet, false, &aux_ptr );
   if (rc == 0) {
      // Interpreted_Capabilities_Fragment contains no pointers, so no need for deep copy
      memcpy(*fragment_ptr, aux_ptr->text, aux_ptr->fragment_length_wo_null+1);
   }
   return rc;
}

#endif



// Table Read


Global_Status_DDC get_interpreted_table_read_fragment(
                     DDC_Packet *  packet,
                     bool          make_copy,
                     Interpreted_Table_Read_Fragment ** interpreted_ptr)
{
   // printf("(%s) Starting\n", __func__);
   Global_Status_DDC rc = DDCRC_OK;
   if (packet->type != DDC_PACKET_TYPE_TABLE_READ_RESPONSE)
      rc = COUNT_STATUS_CODE(DDCRC_RESPONSE_TYPE);
   else {
      if (make_copy) {
         Interpreted_Table_Read_Fragment * copy =
            call_malloc(sizeof(Interpreted_Table_Read_Fragment),
                        "get_interpreted_table_read_fragmente:make_copy");
         memcpy(copy, packet->aux_data, sizeof(Interpreted_Table_Read_Fragment));
         *interpreted_ptr = copy;
      }
      else {
         *interpreted_ptr = packet->aux_data;
      }
   }
   return rc;
}


int get_table_read_offset(DDC_Packet * packet, int * offset_ptr) {
   Interpreted_Table_Read_Fragment * aux_ptr;
   int rc = get_interpreted_table_read_fragment(packet, false, &aux_ptr );
   if (rc == 0) {
      *offset_ptr = aux_ptr->fragment_offset;
   }

   return rc;
}




// VCP Feature Code

Global_Status_DDC get_interpreted_vcp_code(
       DDC_Packet *            packet,
       bool                    make_copy,
       Interpreted_Vcp_Code ** interpreted_ptr)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting\n", __func__);
   Global_Status_DDC rc = DDCRC_OK;
   if (packet->type != DDC_PACKET_TYPE_QUERY_VCP_RESPONSE)
      rc = COUNT_STATUS_CODE(DDCRC_RESPONSE_TYPE);
   else {
      if (make_copy) {
         Interpreted_Vcp_Code * copy =
            call_malloc(sizeof(Interpreted_Vcp_Code), "get_interpreted_vcp_code:make_copy");
         memcpy(copy, packet->aux_data, sizeof(Interpreted_Vcp_Code));
         *interpreted_ptr = copy;
      }
      else {
         *interpreted_ptr = packet->aux_data;
      }
   }
   if (debug)
      printf( "(%s) Returning %d: %s\n", __func__, rc, global_status_code_description(rc) );
   return rc;
}


Global_Status_DDC get_vcp_cur_value(DDC_Packet * packet, int * value_ptr) {
   Interpreted_Vcp_Code * aux_ptr;
   Global_Status_DDC rc = get_interpreted_vcp_code(packet, false, &aux_ptr);
   if (rc == 0) {
      *value_ptr = aux_ptr->cur_value;
   }
   return rc;
}
