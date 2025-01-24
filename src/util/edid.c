/** @file edid.c
 *
 *  Functions to interpret the EDID data structure, irrespective of how
 *  the bytes of the EDID are obtained.
 *
 *  This should be the only source module that understands the internal
 *  structure of the EDID.
 *
 *  While the code here is generic to all EDIDs, only fields of use
 *  to **ddcutil** are interpreted.
 */

// Copyright (C) 2014-2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
#include <inttypes.h>  // printf() format macros for stdint.h
#include <string.h>
#include <syslog.h>
/** \endcond */

#include "debug_util.h"
#include "pnp_ids.h"
#include "report_util.h"
#include "string_util.h"
#include "debug_util.h"

#include "edid.h"


// Direct writes to stdout/stderr: NO


/** Calculates checksum for a 128 byte EDID
 *
 * @param edid pointer to 128 byte EDID block
 * @return checksum byte
 *
 * Note that the checksum byte (offset 127) is itself
 * included in the checksum calculation.
 */
Byte edid_checksum(const Byte * edid) {
   Byte checksum = 0;
   for (int ndx = 0; ndx < 128; ndx++) {
      checksum += edid[ndx];
   }
   return checksum;
}


bool is_valid_edid_checksum(const Byte * edidbytes) {
   return true;
}


bool is_valid_edid_header(const Byte * edidbytes) {
   return true;
}


bool is_valid_raw_edid(const Byte * edid, int len) {
   return (len >= 128) && is_valid_edid_header(edid) && is_valid_edid_checksum(edid);
}


bool is_valid_raw_cea861_extension_block(const Byte * edid, int len) {
   return (len >= 128) && edid[0] == 0x02 && is_valid_edid_checksum(edid);
}


/** Unpacks the 2 byte manufacturer id field from the EDID into a 3 character
 * string.
 *
 * @param   mfg_id_bytes  address of first byte
 * @param   result        address of buffer in which to return result
 * @param   bufsize       buffer size; must be >= 4
 *
 * @remark
 * Since the unpacked value is 4 bytes in length (3 characters plus a trailing '\0')
 * it could easily be returned on the stack.  Consider.
 */
void parse_mfg_id_in_buffer(const Byte * mfg_id_bytes, char * result, int bufsize) {
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


/** Extracts the 3 character manufacturer id from an EDID byte array.
 *  The id is returned with a trailing null in a buffer provided by the caller.
 *
 *  @param  edidbytes    pointer to start of EDID
 *  @param  result       buffer  in which to return manufacturer ID
 *  @param  bufsize      buffer size (must be >= 4)
 */
void get_edid_mfg_id_in_buffer(const Byte* edidbytes, char * result, int bufsize) {
   parse_mfg_id_in_buffer(edidbytes+8, result, bufsize);
}


#define EDID_DESCRIPTORS_BLOCKS_START 54
#define EDID_DESCRIPTOR_BLOCK_SIZE    18
#define EDID_DESCRIPTOR_DATA_SIZE     13
#define EDID_DESCRIPTOR_BLOCK_CT       4


/** Extracts the non-timing descriptors from an EDID, i.e.
 *  ASCII model name, serial number, and other descriptor.
 *  The extracted values are returned as null-terminated strings.
 *
 *  Note that the maximum length of these strings is 13 bytes.
 *
 *  @param  edidbytes        pointer to 128 byte EDID
 *  @param  namebuf          pointer to buffer where model name will be returned.
 *  @param  namebuf_len      size of namebuf, must be >= 14
 *  @param  snbuf            pointer to buffer where serial number will be returned
 *  @param  snbuf_len        size of snbuf, must be >= 14
 *  @param  otherbuf         pointer to buffer where addl descriptor will be returned
 *  @param  otherbuf_len     size of otherbuf, must be >= 14
 *
 * Buffers will be set to "Unspecified" for descriptors that are not found.
 *
 * @remark
 * - Use Buffers as parms instead of pointers and lengths?
 * - Buffer for edidbytes, and just return pointers to newly allocated memory for found strings
 */
static void get_edid_descriptor_strings(
        const Byte* edidbytes,
        char* namebuf,
        int   namebuf_len,
        char* snbuf,
        int   snbuf_len,
        char* otherbuf,
        int   otherbuf_len)
{
   bool debug = false;
   assert(namebuf_len >= 14 && snbuf_len >= 14 && otherbuf_len >= 14);
   strcpy(namebuf,  "");
   strcpy(snbuf,    "");
   strcpy(otherbuf, "");

   int fields_found = 0;

   // 4 descriptor blocks beginning at offset 54.  Each block is 18 bytes.
   // In each block, bytes 0-3 indicates the contents.
   int descriptor_ndx = 0;
   for (descriptor_ndx = 0; descriptor_ndx < EDID_DESCRIPTOR_BLOCK_CT; descriptor_ndx++) {
      const Byte * descriptor = edidbytes +
                          EDID_DESCRIPTORS_BLOCKS_START +
                          descriptor_ndx * EDID_DESCRIPTOR_BLOCK_SIZE;
      if (debug)
         printf("(%s) full descriptor: %s\n",  __func__,
                hexstring(descriptor, EDID_DESCRIPTOR_BLOCK_SIZE));

      // test if a string descriptor
      if ( descriptor[0] == 0x00 &&       // 0x00 if not a timing descriptor
           descriptor[1] == 0x00 &&       // 0x00 if not a timing descriptor
           descriptor[2] == 0x00 &&       // 0x00 for all descriptors
           descriptor[4] == 0x00
         )
      {
         char * nameslot = NULL;
         switch(descriptor[3]) {
         case 0xff:   nameslot = snbuf;     break;      // monitor serial number
         case 0xfe:   nameslot = otherbuf;  break;      // arbitrary ASCII string
         case 0xfc:   nameslot = namebuf;   break;      // monitor name
         }

         if (nameslot) {
            const Byte * textstart = descriptor+5;
            // DBGMSF(debug, "String in descriptor: %s", hexstring(textstart, 14));
            int    offset = 0;
            while (textstart[offset] != 0x0a && offset < 13) {
               nameslot[offset] = textstart[offset];
               offset++;
            }
            nameslot[offset] = '\0';
            rtrim_in_place(nameslot);   // handle no terminating LF but blank padded
            if (debug)
               printf("(%s) name = %s\n", __func__, nameslot);

         fields_found++;
         }
      }
   }
}


/** Parses an EDID.
 *
 * @param edidbytes   pointer to 128 byte EDID block
 *
 * @return pointer to newly allocated Parsed_Edid struct,
 *         or NULL if the bytes could not be parsed.
 *         It is the responsibility of the caller to free this memory.
 *
 * @remark
 * The bytes pointed to by **edidbytes** are copied into the newly
 * allocated Parsed_Edid.  If they were previously malloc'd they
 * need to free'd.
 */
Parsed_Edid * create_parsed_edid(const Byte* edidbytes) {
   assert(edidbytes);
   // bool debug = false;
   Parsed_Edid* parsed_edid = NULL;

   if ( !is_valid_edid_header(edidbytes) || !is_valid_edid_checksum(edidbytes) )
      goto bye;

   parsed_edid = calloc(1,sizeof(Parsed_Edid));
   assert(sizeof(parsed_edid->bytes) == 128);
   memcpy(parsed_edid->marker, EDID_MARKER_NAME, 4);
   memcpy(parsed_edid->bytes,  edidbytes, 128);

   get_edid_mfg_id_in_buffer(
           edidbytes,
           parsed_edid->mfg_id,
           sizeof(parsed_edid->mfg_id) );

   parsed_edid->product_code = edidbytes[0x0b] << 8 | edidbytes[0x0a];

   parsed_edid->serial_binary = edidbytes[0x0c]       |
                                edidbytes[0x0d] <<  8 |
                                edidbytes[0x0e] << 16 |
                                edidbytes[0x0f] << 24;

   get_edid_descriptor_strings(
           edidbytes,
           parsed_edid->model_name,   sizeof(parsed_edid->model_name),
           parsed_edid->serial_ascii, sizeof(parsed_edid->serial_ascii),
           parsed_edid->extra_descriptor_string, sizeof(parsed_edid->extra_descriptor_string)
           );

   parsed_edid->year = edidbytes[17] + 1990;
   parsed_edid->is_model_year = edidbytes[16] == 0xff;
   parsed_edid->manufacture_week = edidbytes[16];
   parsed_edid->edid_version_major = edidbytes[18];
   parsed_edid->edid_version_minor = edidbytes[19];

   parsed_edid->rx = edidbytes[0x1b] << 2 | ( (edidbytes[0x19]&0b11000000)>>6 );
   parsed_edid->ry = edidbytes[0x1c] << 2 | ( (edidbytes[0x19]&0b00110000)>>4 );
   parsed_edid->gx = edidbytes[0x1d] << 2 | ( (edidbytes[0x19]&0b00001100)>>2 );
   parsed_edid->gy = edidbytes[0x1e] << 2 | ( (edidbytes[0x19]&0b00000011)>>0 );
   parsed_edid->bx = edidbytes[0x1f] << 2 | ( (edidbytes[0x1a]&0b11000000)>>6 );
   parsed_edid->by = edidbytes[0x20] << 2 | ( (edidbytes[0x1a]&0b00110000)>>4 );
   parsed_edid->wx = edidbytes[0x21] << 2 | ( (edidbytes[0x1a]&0b00001100)>>2 );
// parsed_edid->wy = edidbytes[0x22] << 2 | ( (edidbytes[0x1a]&0b00000011)>>0 );
// low order digits wrong, try another way
   parsed_edid->wy = edidbytes[0x22] * 4 + ((edidbytes[0x1a]&0b00000011)>>0);

   parsed_edid->video_input_definition = edidbytes[0x14];
   // printf("(%s) video_input_parms_bitmap = 0x%02x\n", __func__, video_input_parms_bitmap);
   // parsed_edid->is_digital_input = (parsed_edid->video_input_definition & 0x80) ? true : false;

   parsed_edid->supported_features = edidbytes[0x18];
   parsed_edid->extension_flag = edidbytes[0x7e];

bye:
   return parsed_edid;
}


/** Parses an EDID and sets the edid_source field.
 *
 * @param edidbytes   pointer to 128 byte EDID block
 * @param source      source of EDID, typically I2C but may be X11,
 *                    USB, SYSFS, DRM
 *
 * @return pointer to newly allocated Parsed_Edid struct,
 *         or NULL if the bytes could not be parsed.
 *         It is the responsibility of the caller to free this memory.
 *
 * **source** is copied to the Parsed_Edid and should be freed is it
 * was allocated.
 */
Parsed_Edid * create_parsed_edid2(const Byte* edidbytes, const char * source) {
   Parsed_Edid * edid = create_parsed_edid(edidbytes);
   if (edid) {
      assert(source && strlen(source) < EDID_SOURCE_FIELD_SIZE);
      STRLCPY(edid->edid_source, source, EDID_SOURCE_FIELD_SIZE);
   }
   return edid;
}


Parsed_Edid * copy_parsed_edid(Parsed_Edid * original) {
   bool debug = false;
   DBGF(debug, "Starting. original=%p", original);
   Parsed_Edid * copy =  NULL;
   if (original) {
      // it's easier to simply reparse the bytes we know successfully parsed
      // than to perform a deep copy
      copy = create_parsed_edid(original->bytes);
      assert(copy);
      // the one field that won't have been reparsed
      memcpy(&copy->edid_source, original->edid_source, sizeof(original->edid_source));
      // report_parsed_edid(copy, true, 2);
   }
   DBGF(debug, "Done. returning %p -> ", copy);
   return copy;
}


/** Frees a Parsed_Edid struct.
 *
 * @param  parsed_edid  pointer to Parsed_Edid struct to free
 */
void free_parsed_edid(Parsed_Edid * parsed_edid) {
   bool debug = false;
   assert( parsed_edid );
   // show_backtrace(1);
   DBGF(debug, "(free_parsed_edid) parsed_edid=%p", parsed_edid);
   // ASSERT_WITH_BACKTRACE(memcmp(parsed_edid->marker, EDID_MARKER_NAME, 4)==0);
   if ( memcmp(parsed_edid->marker, EDID_MARKER_NAME, 4)==0 ) {
      parsed_edid->marker[3] = 'x';
      // n. Parsed_Edid contains no pointers
      free(parsed_edid);
   }
   else {
      char * s = g_strdup_printf("Invalid free of Parsed_Edid@%p, marker=%s",
            parsed_edid, hexstring_t((unsigned char *) parsed_edid->marker, 4));
      DBGF(true, "%s", s);
      syslog(LOG_USER|LOG_ERR, "(%s) %s", __func__, s);
      free(s);
   }
}


// TODO: generalize to base_asciify(char* s, char* prefix, char* suffix)
//       move to string_util.h

/** Replaces every character in a string whose value is > 127 with
 *  the string "<xHH>", where HH is the hex value of the character.
 *
 *  @param   s  string to convert
 *  @return  newly allocated modified character string
 *
 *  The caller is responsible for freeing the returned string
 */
char * base_asciify(char * s) {
   int badct = 0;
   int ndx = 0;
   while (s[ndx]) {
      if (s[ndx] & 0x80 || s[ndx] < 32)
         badct++;
      ndx++;
   }
   int reqd = ndx + 1 + 4*badct;
   char* result = malloc(reqd);
   int respos = 0;
   ndx = 0;
   while(s[ndx]) {
      // printf("s[ndx] = %u\n", (unsigned char)s[ndx]);
      bool is_printable = s[ndx] >=32 && s[ndx] < 128 ;
      if ( is_printable) {
         result[respos++] = s[ndx];
      }
      else {
         sprintf(result+respos, "<x%02x>", (unsigned char) s[ndx]);
         respos += 5;
      }
      ndx++;
   }
   result[respos] = '\0';
   // printf("respos=%d, reqd=%d\n", respos, reqd);
   assert(respos == (reqd-1));
   return result;
}


/** Replaces every character in a string whose value is > 127 with
 *  the string "<xHH>", where HH is the hex value of the character.
 *
 *  @param   s  string to convert
 *  @return  converted string
 *
 *  The returned value is valid until the next call to this function
 *  in the current thread.
 */
char * base_asciify_t(char * s) {
   static GPrivate  x_key = G_PRIVATE_INIT(g_free);
   static GPrivate  x_len_key = G_PRIVATE_INIT(g_free);

   char * buftemp = base_asciify(s);
   char * buf = get_thread_dynamic_buffer(&x_key, &x_len_key, strlen(s)+1);
   strcpy(buf, buftemp);
   free(buftemp);
   return buf;
}


/** Writes EDID summary to the current report output destination.
 * (normally stdout, but may be changed by rpt_push_output_dest())
 *
 *  @param  edid              pointer to parsed edid struct
 *  @param  verbose_synopsis  show additional EDID detail
 *  @param  show_raw          include hex dump of EDID
 *  @param  depth             logical indentation depth
 *
 *  @remark
 *  Output is written using rpt_ functions.
 */
void report_parsed_edid_base(
      Parsed_Edid *  edid,
      bool           verbose_synopsis,
      bool           show_raw,
      int            depth)
{
   bool debug = false;
   if (debug)
      printf("(%s) Starting. edid=%p, verbose_synopsis=%s, show_raw=%s\n",
             __func__, (void*)edid, SBOOL(verbose_synopsis), sbool(show_raw));

   if (debug) {
      show_backtrace(2);
      if (redirect_reports_to_syslog)
         backtrace_to_syslog(LOG_NOTICE, 2);
   }


   int d1 = depth+1;
   int d2 = depth+2;
   // verbose = true;
   if (edid) {
      rpt_vstring(depth,"EDID synopsis:");
      rpt_vstring(d1,"Mfg id:               %s - %s",     edid->mfg_id, pnp_name(edid->mfg_id));
      rpt_vstring(d1,"Model:                %s",          base_asciify_t(edid->model_name));
      rpt_vstring(d1,"Product code:         %u  (0x%04x)", edid->product_code, edid->product_code);
   // rpt_vstring(d1,"Product code:         %u",          edid->product_code);
      rpt_vstring(d1,"Serial number:        %s",          base_asciify_t(edid->serial_ascii));
      // Binary serial number is typically 0x00000000 or 0x01010101, but occasionally
      // useful for differentiating displays that share a generic ASCII "serial number"
      rpt_vstring(d1,"Binary serial number: %"PRIu32" (0x%08x)", edid->serial_binary, edid->serial_binary);
      if (edid->is_model_year)
      rpt_vstring(d1,"Model year:           %d", edid->year);
      else
      rpt_vstring(d1,"Manufacture year:     %d,  Week: %d", edid->year, edid->manufacture_week);
      if (verbose_synopsis) {
         rpt_vstring(d1,"EDID version:         %d.%d",       edid->edid_version_major, edid->edid_version_minor);
#ifdef TEST_ASCIIFY
         char bad[] = {0x81, 0x32, 0x83, 0x34, 0x85, 0x00};
         strcpy(edid->extra_descriptor_string, bad);
#endif
         rpt_vstring(d1,"Extra descriptor:        %s",          base_asciify_t(edid->extra_descriptor_string));
         char explbuf[100];
         explbuf[0] = '\0';
         if (edid->video_input_definition & 0x80) {
            strcpy(explbuf, "Digital Input");
            if (edid->edid_version_major == 1 && edid->edid_version_minor >= 4) {
               switch (edid->video_input_definition & 0x0f) {
               case 0x00:
                  strcat(explbuf, " (Digital interface not defined)");
                  break;
               case 0x01:
                  strcat(explbuf, " (DVI)");
                  break;
               case 0x02:
                  strcat(explbuf, " (HDMI-a)");
                  break;
               case 0x03:
                  strcat(explbuf, " (HDMI-b");
                  break;
               case 0x04:
                  strcat(explbuf, " (MDDI)");
                  break;
               case 0x05:
                  strcat(explbuf, " (DisplayPort)");
                  break;
               default:
                  strcat(explbuf, " (Invalid DVI standard)");
               }
               strcat(explbuf, ", Bit depth: ");
               switch ( (edid->video_input_definition & 0x70) >> 4) {
               case 0x00:
                  strcat(explbuf, "undefined");
                  break;
               case 0x01:
                  strcat(explbuf, "6");
                  break;
               case 0x02:
                  strcat(explbuf, "8");
                  break;
               case 0x03:
                  strcat(explbuf, "10");
                  break;
               case 0x04:
                  strcat(explbuf, "12");
                  break;
               case 0x05:
                  strcat(explbuf, "14");
                  break;
               case 0x06:
                  strcat(explbuf, "16");
                  break;
               case 0x07:
                  strcat(explbuf, " (x07 reserved)");
               }
            }
         }
         else {
            strcpy(explbuf, "Analog Input");
         }
         rpt_vstring(d1,"Video input definition:    0x%02x - %s", edid->video_input_definition, explbuf);
         // end, video_input_definition interpretation

         rpt_vstring(d1, "Supported features:");
         if (edid->supported_features & 0x80)
            rpt_vstring(d2, "DPMS standby");
         if (edid->supported_features & 0x40)
            rpt_vstring(d2, "DPMS suspend");
         if (edid->supported_features & 0x20)
            rpt_vstring(d2, "DPMS active-off");
         Byte display_type = (edid->supported_features & 0x18) >> 3;     // bits 4-3
         // printf("(%s) supported_features = 0x%02x, display_type = 0x%02x=%d\n",
         //       __func__, edid->supported_features, display_type, display_type);
         if (edid->video_input_definition & 0x80) {   // digital input
            switch(display_type) {
            case 0:
               rpt_vstring(d2, "Digital display type: RGB 4:4:4");
               break;
            case 1:
               rpt_vstring(d2, "Digital display type: RGB 4:4:4 + YCrCb 4:4:4");
               break;
            case 2:
               rpt_vstring(d2, "Digital display type: RGB 4:4:4 + YCrCb 4:2:2");
               break;
            case 3:
               rpt_vstring(d2, "Digital display type: RGB 4:4:4 + YCrCb 4:4:4 + YCrCb 4:2:2");
               break;
            default:
               rpt_vstring(d2, "Invalid digital display type: 0x%02x", display_type);
            }
         }
         else {   // analog input
            switch(display_type) {
            case 0:
               rpt_vstring(d2, "Analog display type: Monochrome or grayscale");
               break;
            case 1:
               rpt_vstring(d2, "Analog display type: Color");
               break;
            case 2:
               rpt_vstring(d2, "Analog display type: Non-RGB color");
               break;
            case 3:
               rpt_vstring(d2, "Undefined analog display type");
               break;
            default:
               // should be PROGRAM_LOGIC_ERROR, but that would violate layering
               rpt_vstring(d2, "Invalid analog display type: 0x%02x", display_type);
            }
         }
         rpt_vstring(d2, "Standard sRGB color space: %s", (display_type & 0x02) ? "True" : "False");
         // end supported_features interpretation

         rpt_vstring(d1,"White x,y:        %.3f, %.3f",  edid->wx/1024.0, edid->wy/1024.0);
         rpt_vstring(d1,"Red   x,y:        %.3f, %.3f",  edid->rx/1024.0, edid->ry/1024.0);
         rpt_vstring(d1,"Green x,y:        %.3f, %.3f",  edid->gx/1024.0, edid->gy/1024.0);
         rpt_vstring(d1,"Blue  x,y:        %.3f, %.3f",  edid->bx/1024.0, edid->by/1024.0);

         // restrict to EDID version >= 1.3?
         rpt_vstring(d1,"Extension blocks: %u",    edid->extension_flag);

         if (strlen(edid->edid_source) > 0)   // will be set only for USB devices, values "USB", "X11"
            rpt_vstring(depth,"EDID source: %s",        edid->edid_source);
      }  // if (verbose_synopsis)
      if (show_raw) {
         rpt_vstring(depth,"EDID hex dump:");
         rpt_hex_dump(edid->bytes, 128, d1);
      }

   }  // if (edid)
   else {
       if (verbose_synopsis)
         rpt_vstring(d1,"No EDID");
   }

   if (debug)
      printf("(%s) Done.\n", __func__);
}


/** Reports whether this is an analog or digital display.
 *
 *  @param edid    pointer to parse edid struct
 *  @retval false  analog display
 *  @retval true   digital display
 */
bool is_input_digital(Parsed_Edid * edid) {
   return edid->video_input_definition & 0x80;
}


/** Writes a summary of an EDID to the current report output destination.
 * (normally stdout, but may be changed by rpt_push_output_dest())
 *
 *  @param  edid       pointer to parsed edid struct
 *  @param  verbose    include hex dump of EDID
 *  @param  depth      logical indentation depth
 */
void report_parsed_edid(Parsed_Edid * edid, bool verbose, int depth) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting. verbose=%s\n", __func__, SBOOL(verbose));
   report_parsed_edid_base(edid, verbose, verbose, depth);
   if (debug)
      printf("(%s) Done.\n", __func__);
}


/** Heuristic test for a laptop display.  Observed laptop displays
 *  never have the model name and serial numbers set.
 */
bool is_laptop_parsed_edid(Parsed_Edid * parsed_edid) {
   assert(parsed_edid);
   // 12/10/2024: seen laptop screen w. model name but not serial_ascii
   bool result = streq(parsed_edid->model_name,  "") &&
                 streq(parsed_edid->serial_ascii,"");
   return result;
}

