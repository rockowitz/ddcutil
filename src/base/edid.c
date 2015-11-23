/*
 * raw_edid.c
 *
 *  Created on: Oct 17, 2015
 *      Author: rock
 *
 *  Functions for processing the EDID data structure, irrespective of how
 *  the bytes of the EDID are obtained.
 *
 *  This should be the only source module that understands the internal
 *  structure of the EDID.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <util/report_util.h>
#include <util/string_util.h>
#include <base/util.h>       // used only by debugging code

#include <base/edid.h>


/* Calculates checksum for a 128 byte EDID
 *
 * Note that the checksum byte itself (byte 127) is itself
 * included in the checksum calculation.
 */
Byte edid_checksum(Byte * edid) {
   Byte checksum = 0;
   int ndx = 0;
   for (ndx = 0; ndx < 128; ndx++) {
      checksum += edid[ndx];
   }
   return checksum;
}


/* Unpacks the 2 byte manufacturer id field from the EDID into a 3 character
 * string.
 *
 * Arguments:
 *    mfg_id_bytes  address of first byte
 *    result        address of buffer in which to return result
 *    bufsize       buffer size; must be >= 4
 *
 * Returns:
 *    nothing
 */
void parse_mfg_id_in_buffer(Byte * mfg_id_bytes, char * result, int bufsize) {
      assert(bufsize >= 4);
      result[0] = (mfg_id_bytes[0] >> 2) & 0x1f;
      result[1] = ((mfg_id_bytes[0] & 0x03) << 3) | ((mfg_id_bytes[1] >> 5) & 0x07);
      result[2] = mfg_id_bytes[1] & 0x1f;
      // printf("result[0] = 0x%02x\n", result[0]);
      // printf("result[1] = 0x%02x\n", result[1]);
      // printf("result[2] = 0x%02x\n", result[2]);
      result[0] += 64;
      result[1] += 64;
      result[2] += 64;
      result[3] = 0;        // terminating null
}


// Extracts the 3 character manufacturer id from an EDID byte array.
// The id is returned with a trailing null in a buffer provided by the caller.
void get_edid_mfg_id_in_buffer(Byte* edidbytes, char * result, int bufsize) {
   parse_mfg_id_in_buffer(&edidbytes[8], result, bufsize);
}


#ifdef OLD

// Extracts the 3 character manufacturer id from an EDID byte array.
//
// Note it is the caller's responsibility to free the buffer returned.

char * get_edid_mfg_id(Byte * edidbytes) {
   char * result = call_malloc(4, "get_mfg_id");

   get_edid_mfg_id_in_buffer(edidbytes, result);
   return result;
}

#endif


#define EDID_DESCRIPTORS_BLOCKS_START 54
#define EDID_DESCRIPTOR_BLOCK_SIZE    18
#define EDID_DESCRIPTOR_BLOCK_CT       4


/* Extracts the ASCII model name and serial number from an EDID.
 *
 * Note that the maximum length of these strings is 13 bytes.
 *
 * Returns:
 *    true if both fields found, false if not
 */

// Use Buffer instead of pointers and lengths?

bool get_edid_modelname_and_sn(
        Byte* edidbytes,
        char* namebuf,
        int   namebuf_len,
        char* snbuf,
        int   snbuf_len)
{
   assert(namebuf_len >= 14);
   assert(snbuf_len >= 14);

   int fields_found = 0;

   // 4 descriptor blocks beginning at offset 54.  Each block is 18 bytes.
   // In each block, bytes 0-3 indicates the contents.
   int descriptor_ndx = 0;
   for (descriptor_ndx = 0; descriptor_ndx < EDID_DESCRIPTOR_BLOCK_CT; descriptor_ndx++) {
      Byte * descriptor = edidbytes +
                          EDID_DESCRIPTORS_BLOCKS_START +
                          descriptor_ndx * EDID_DESCRIPTOR_BLOCK_SIZE;
      // printf("full descriptor: %s\n", hexstring(descriptor, EDID_DESCRIPTOR_BLOCK_SIZE));
      // printf("descriptor[0] = 0x%02x\n", descriptor[0]);
      // printf("descriptor[1] = 0x%02x\n", descriptor[1]);
      // printf("descriptor[3] = 0x%02x\n", descriptor[3]);
      if ( descriptor[0] == 0x00 &&       // 0x00 if not a timing descriptor
           descriptor[1] == 0x00 &&       // 0x00 if not a timing descriptor
           descriptor[2] == 0x00 &&       // 0x00 for all descriptors
          (descriptor[3] == 0xff || descriptor[3] == 0xfc)  // 0xff: serial number, 0xfc: model name
         )
      {
         // char * nametype = (descriptor[3] == 0xff) ? "Serial number" : "Model name";
         char * nameslot = (descriptor[3] == 0xff) ? snbuf : namebuf;
         Byte * textstart = descriptor+5;
         // printf("String in descriptor: %s\n", hexstring(textstart, 14));
         int    textlen = 0;
         while (*(textstart+textlen) != 0x0a && textlen < 14) {
            // printf("(%s) textlen=%d, char=0x%02x\n", __func__, textlen, *(textstart+textlen));
            textlen++;
         }
         memcpy(nameslot, textstart, textlen);
         nameslot[textlen] = '\0';
         // printf("(%s) name = %s\n", __func__, nameslot);
         fields_found++;
      }
   }
   return (fields_found == 2);
}

#ifdef REFERENCE
typedef
struct {
   char         marker[4];          // always "EDID"
   Byte         bytes[128];
   char         mfg_id[4];
   char         model_name[14];
   char         serial_ascii[14];
} Parsed_Edid;
#endif


Parsed_Edid * create_parsed_edid(Byte* edidbytes) {
   // TODO: implement depth
   assert(edidbytes);
   bool        ok;
   Parsed_Edid* parsed_edid = NULL;

   parsed_edid = calloc(1,sizeof(Parsed_Edid));
   assert(sizeof(parsed_edid->bytes) == 128);
   memcpy(parsed_edid->marker, EDID_MARKER_NAME, 4);
   memcpy(parsed_edid->bytes,  edidbytes, 128);

   get_edid_mfg_id_in_buffer(
           edidbytes,
           parsed_edid->mfg_id,       sizeof(parsed_edid->mfg_id) );

   ok = get_edid_modelname_and_sn(
           edidbytes,
           parsed_edid->model_name,   sizeof(parsed_edid->model_name),
           parsed_edid->serial_ascii, sizeof(parsed_edid->serial_ascii) );

   parsed_edid->year = edidbytes[17] + 1990;
   parsed_edid->is_model_year = edidbytes[16] == 0xff;
   parsed_edid->edid_version_major = edidbytes[18];
   parsed_edid->edid_version_minor = edidbytes[19];

   if (!ok) {
      free(parsed_edid);
      parsed_edid = NULL;
   }
   return parsed_edid;
}


void report_parsed_edid(Parsed_Edid * edid, bool verbose, int depth) {
   int d1 = depth+1;
   // verbose = true;
   if (edid) {
      rpt_vstring(depth,"EDID synopsis:");

      rpt_vstring(d1,"Mfg id:           %s",          edid->mfg_id);
      rpt_vstring(d1,"Model:            %s",          edid->model_name);
      rpt_vstring(d1,"Serial number:    %s",          edid->serial_ascii);
      char * title = (edid->is_model_year) ? "Model year" : "Manufacture year";
      rpt_vstring(d1,"%-16s: %d", title, edid->year);
      rpt_vstring(d1,"EDID version:     %d.%d", edid->edid_version_major, edid->edid_version_minor);

      if (verbose) {
         rpt_vstring(d1,"EDID hex dump:");
         hex_dump(edid->bytes, 128);
      }
   }
   else {
      if (verbose)
         rpt_vstring(d1,"(%s) edid == NULL", __func__ );
   }
}

