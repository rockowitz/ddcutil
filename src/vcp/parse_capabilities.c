/* ddc_capabilities.c
 *
 * Parse the capabilities string
 *
 * <copyright>
 * Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
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
 *  Parse the capabilities string returned by DDC, query the parsed data structure.
 */

/** \cond */
#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
/** \endcond */

#include "util/report_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/displays.h"

#include "vcp/ddc_command_codes.h"
#include "vcp/parsed_capabilities_feature.h"
#include "vcp/vcp_feature_codes.h"

#include "vcp/parse_capabilities.h"


#ifdef TESTS
// not made static to avoid warning about unused variable
char* test_cap_strings[] = {
      // GSM LG Ultra HD
      "(prot(monitor)type(LED)model(25UM65)cmds(01 02 03 0C E3 F3)"
      "vcp(0203(10 00)0405080B0C101214(05 07 08 0B) 16181A5260(03 04)6C6E70"
      "87ACAEB6C0C6C8C9D6(01 04)DFE4E5E6E7E8E9EAEBED(00 10 20 40)EE(00 01)"
      "FE(01 02 03)FF)mswhql(1)mccs_ver(2.1))",
};
#endif


//
// Report parsed data structures
//

static void report_commands(Byte_Value_Array cmd_ids, int depth) {
   rpt_label(depth, "Commands:");
   int ct = bva_length(cmd_ids);
   int ndx = 0;
   for (; ndx < ct; ndx++) {
      Byte hval = bva_get(cmd_ids, ndx);
      rpt_vstring(depth+1, "Command: %02x (%s)", hval, ddc_cmd_code_name(hval));
   }
}


static void report_features(
      GPtrArray*             features,     // GPtrArray of Capabilities_Feature_Record
      DDCA_MCCS_Version_Spec vcp_version)
{
   bool debug = false;
   int d0 = 0;
   int d1 = 1;

   rpt_label(d0, "VCP Features:");
   int ct = features->len;
   int ndx;
   for (ndx=0; ndx < ct; ndx++) {
      Capabilities_Feature_Record * vfr = g_ptr_array_index(features, ndx);
      DBGMSF(debug, "vfr = %p", vfr);
      report_capabilities_feature(vfr, vcp_version, d1);
   }
}


/** Reports the Parsed_Capabilities struct for human consumption.
 *
 * @param pcaps pointer to ***Parsed_Capabilities***
 *
 * Output is written to the current stdout device.
 */
void report_parsed_capabilities(Parsed_Capabilities* pcaps)
{
   bool debug = false;
   assert(pcaps && memcmp(pcaps->marker, PARSED_CAPABILITIES_MARKER, 4) == 0);
   DBGMSF(debug, "Starting. pcaps->raw_cmds_segment_seen=%s, pcaps->commands=%p, pcaps->vcp_features=%p",
          bool_repr(pcaps->raw_cmds_segment_seen), pcaps->commands, pcaps->vcp_features);

   int d0 = 0;
   // int d1 = d0+1;
   DDCA_Output_Level output_level = get_output_level();
   if (output_level >= DDCA_OL_VERBOSE) {
      rpt_vstring(d0, "%s capabilities string: %s",
                      (pcaps->raw_value_synthesized) ? "Synthesized unparsed" : "Unparsed",
                      pcaps->raw_value);
   }
   bool damaged = false;
   rpt_vstring(d0, "MCCS version: %s",
                   (pcaps->mccs_version_string) ? pcaps->mccs_version_string : "not present");

   if (pcaps->commands)
      report_commands(pcaps->commands, d0);
   else {
      // not an error in the case of USB_IO, as the capabilities string was
      // synthesized and does not include a commands segment
      // also, HP LP2480zx does not have cmds segment
      if (pcaps->raw_cmds_segment_seen)
         damaged = true;
   }
   if (pcaps->vcp_features)
      report_features(pcaps->vcp_features, pcaps->parsed_mccs_version);
   else {
      // handle pathological case of 0 length capabilities string, e.g. Samsung S32D850T
      if (pcaps->raw_vcp_features_seen)
         damaged = true;
   }
   if (damaged)
      rpt_label(d0, "Capabilities string not completely parsed");
}


//
// Lifecycle
//

/* Creates a Parsed_Capabilities record.
 *
 * The data structures passed to this function become owned by
 * the newly created Parsed_Capabilties record.
 *
 * Arguments:
 *    raw_value
 *    mccs_ver
 *    raw_cmds_segment_seen
 *    commands
 *    vcp_features
 *
 * Returns:
 *    Parsed_Capaibilities record
 */
Parsed_Capabilities * new_parsed_capabilities(
      char *            raw_value,
      char *            mccs_ver,
      bool              raw_cmds_segment_seen,
      bool              raw_vcp_features_seen,
      Byte_Value_Array  commands,         // each stored byte is command id
      GPtrArray *       vcp_features
     )
{
   bool debug = false;
   DBGMSF(debug, "raw_cmds_segment_seen=%s, commands=%p", bool_repr(raw_cmds_segment_seen), commands);
   Parsed_Capabilities* pcaps = calloc(1, sizeof(Parsed_Capabilities));
   memcpy(pcaps->marker, PARSED_CAPABILITIES_MARKER, 4);
   pcaps->raw_value             = raw_value;
   pcaps->mccs_version_string   = mccs_ver;
   pcaps->raw_cmds_segment_seen = raw_cmds_segment_seen,
   pcaps->commands              = commands;
   pcaps->vcp_features          = vcp_features;

   DDCA_MCCS_Version_Spec parsed_vcp_version = {0.0};
   if (mccs_ver) {
      int vmajor;
      int vminor;
      int rc = sscanf(mccs_ver, "%d.%d", &vmajor, &vminor);
      if (rc != 2) {
         DBGMSG("Unable to parse mccs_ver, value=\"%s\", rc=%d\n", mccs_ver, rc);
      }
      else {
         parsed_vcp_version.major = vmajor;
         parsed_vcp_version.minor = vminor;
         // DBGMSG("Parsed mccs_ver: %d.%d", parsed_vcp_version.major, parsed_vcp_version.minor);
      }
   }
   pcaps->parsed_mccs_version = parsed_vcp_version;

   return pcaps;
}


/** Frees a Parsed_Capabilities record
 *
 * @param pcaps  pointer to #Parsed_Capabilities struct
 */
void free_parsed_capabilities(Parsed_Capabilities * pcaps) {
   bool debug = false;
   DBGMSF(debug, "Starting. pcaps=%p", pcaps);

   assert( pcaps );
   assert( memcmp(pcaps->marker, PARSED_CAPABILITIES_MARKER, 4) == 0);

   free(pcaps->raw_value);
   free(pcaps->mccs_version_string);

   if (pcaps->commands)
      bva_free(pcaps->commands);

   if (pcaps->vcp_features) {
      DBGMSF(debug, "vcp_features->len = %d", pcaps->vcp_features->len);
      int ndx;
      for (ndx=pcaps->vcp_features->len-1; ndx >=0; ndx--) {
         Capabilities_Feature_Record * vfr =
               g_ptr_array_index(pcaps->vcp_features, ndx);
         // report_feature(vfr);
         free_capabilities_feature(vfr);
         g_ptr_array_remove_index(pcaps->vcp_features, ndx);
      }
      g_ptr_array_free(pcaps->vcp_features, true);
   }

   pcaps->marker[3] = 'x';
   free(pcaps);
}



//
// Parsing
//


/* Capabilities string format.

   Parenthesized expression
   containing sequence of "segments"
   each segment consists of a segment name, followed by a parenthesized value

 */


typedef
struct {
   char * name_start;
   int    name_len;
   char * value_start;
   int    value_len;
   char * remainder_start;
   int    remainder_len;
} Capabilities_Segment;


/* Extract information about the next segment of the capabilities string.
 *
 * Arguments:     start   current position in the capabilities string
 *                len     length of remainder of capabilities string
 *
 * Returns:    pointer to newly allocated Capabilities_Segment describing the segment
 *             It is the responsibility of the caller to free the returned struct,
 *             BUT NOT THE LOCATIONS IT ADDRESSES
 */
static
Capabilities_Segment * next_capabilities_segment(char * start, int len) {
   Capabilities_Segment * segment = calloc(1, sizeof(Capabilities_Segment));

   char * pos = start;
   while(*pos != '(') pos++;
   segment->name_start = start;
   // Fix for Apple Cinema Display, which precedes segment name with blank
   while(segment->name_start < pos && *segment->name_start == ' ') segment->name_start++;
   segment->name_len = pos-(segment->name_start);
   int depth = 1;
   segment->value_start = pos+1;
   while (depth > 0) {
      pos = pos + 1;
      if (*pos == '(')
         depth++;
      else if (*pos == ')')
         depth--;
   }
   segment->value_len = pos - segment->value_start;
   segment->remainder_start = pos+1;
   segment->remainder_len = start + len - segment->remainder_start;

   // printf("name:      |%.*s|\n", segment->name_len, segment->name_start);
   // printf("value:     |%.*s|\n", segment->value_len, segment->value_start);
   // printf("remainder: |%.*s|\n", segment->remainder_len, segment->remainder_start);

   return segment;
}


// TODO: On every monitor tested, the values are separated by spaces.
// However, per the Access Bus spec, Section 7, values need not be separated by spaces,
// e.g.  010203 is valid

/* Parse the value of the cmds segment, which is a list of
 * 2 character hex values separated by spaces.
 *
 * Arguments:
 *    start
 *    len
 *
 * Returns:
 *    Byte_Value_Array indicating command values seen
 */
//  Alternatively, return a ByteBitFlag instance,
//  or pass a preallocted instances
static Byte_Value_Array parse_cmds_segment(
      char * start,
      int    len)
{
   bool debug = false;
   DBGMSF(debug, "Starting. start=%p, len=%d", start, len);

   Byte_Value_Array cmd_ids2 = bva_create();
   bool ok = store_bytehex_list(start, len, cmd_ids2, bva_appender);
   if (!ok) {
      f0printf(ferr(), "Error processing commands list: %.*s\n", len, start);
   }
   // report_id_array(cmd_ids, "Command ids found:");
   if (debug) {
      // report_cmd_array(cmd_ids);
      DBGMSG("store_bytehex_list returned %d", ok);
      report_commands(cmd_ids2, 1);
   }
   Byte_Value_Array result = (ok) ? cmd_ids2 : NULL;
   DBGMSF(debug, "returning %p", result);
   return result;
}


/* Finds the matching closing parenthesis for the
 * current open parenthesis.
 *
 * Arguments:
 *   start    first character to examine (must be '(')
 *   end      points to end of string, i.e. the byte
 *            after the last character to examine
 *
 * Returns:
 *   pointer to closing parenthesis
 *   or end if closing parenthesis not found
 */

static char * find_closing_paren(
      char * start,
      char * end)
{
   assert( *start == '(');
   char * pos = start+1;
   int depth = 1;
   while (pos < end && depth > 0) {
      if (*pos == '(')
         depth++;
      else if (*pos == ')')
         depth--;
      pos++;
   }
   if (depth == 0)
      pos--;
   return pos;
}


/* Parse the value of the vcp segment.
 *
 * Arguments:
 *    start
 *    len
 *
 * Returns:
 *    GPtrArray of Capabilities_Feature_Record *
 */
static GPtrArray * parse_vcp_segment(
      char * start,
      int    len)
{
   bool debug = false;
   GPtrArray* vcp_array = g_ptr_array_sized_new(40);              // initial size
   // Vcp_Code_Table_Entry * vcp_entry;    // future?
   char * pos = start;
   char * end = start + len;
   Byte   cur_feature_id = 0x00; // initialization logically unnecessary, but o.w. get warning
   bool   valid_feature;
   int    value_len = 0;         // initialization logically unnecessary, but o.w. get warning
   char * value_start = NULL;    // ditto
   while (pos < end) {
      valid_feature = false;
      // strip leading blanks
      while(*pos == ' ' && pos < end) pos++;
      if (pos == end)
         break;

      char * st = pos;
      while (*pos != ' ' && *pos != '(' && pos < end) pos++;
      int len = pos-st;
      DBGMSF(debug, "Found: Feature code subsegment: %.*s\n", len, st);
      // If len > 2, feature codes not separated by blanks.  Take just the first 2 characters
      if (len > 2) {
         pos = st + 2;
         len = 2;
      }
      bool value_ok = false;
      if (len == 2) {
         // cur_feature_id = hhc_to_byte(st);   // what if invalid hex?
         value_ok = hhc_to_byte_in_buf(st, &cur_feature_id);
         if (value_ok) {
            valid_feature = true;
            value_start = NULL;
            value_len   = 0;
         }
      }
      if (!value_ok) {
         f0printf(ferr(), "Feature: %.*s (invalid code)\n", 1, st);
      }

      if (*pos == '(') {
         // find matching )
         char * value_end = find_closing_paren(pos, end);
         if (value_end == end) {
            DBGMSG0("Value parse terminated without closing paren   " );
            // bad data, what to do?
            // need better error message
            // TODO: recover from error, this is bad data from the monitor
            goto bye;
         }
         value_start = pos+1;
         value_len = value_end - (pos + 1);
         // printf("  Values: %.*s\n", value_len, value_start);
         pos = value_end + 1;   // point to character after closing paren
      }

      if (valid_feature) {
         Capabilities_Feature_Record * vfr =
               new_capabilities_feature(cur_feature_id, value_start, value_len);
         if (debug) {
            DDCA_MCCS_Version_Spec dummy_version = {0,0};
            report_capabilities_feature(vfr, dummy_version, 1);
         }
         g_ptr_array_add(vcp_array, vfr);
      }
   }
bye:
   return vcp_array;
}


/** Parses the entire capabilities string
 *
 *  @param  buf_start   starting address of string
 *  @param  buf_len     length of string (not including trailing null)
 *
 *  @return pointer to newly allocated ParsedCapabilities structure
 */
Parsed_Capabilities * parse_capabilities(
      char * buf_start,
      int    buf_len)
{
   // DBGMSG("Substituting test capabilities string");
   // buf_start = test_cap_strings[0];
   // buf_len = strlen(test_cap_strings[0]);

   bool debug = false;
   if (debug) {
      DBGMSG("Starting. len=%d", buf_len);
      hex_dump((Byte*)buf_start, buf_len);
      DBGMSG("Starting. buf_start -> |%.*s|", buf_len, buf_start);
   }

   // Make a copy of the unparsed value
   char * raw_value = chars_to_string(buf_start, buf_len);

   char * mccs_ver_string  = NULL;
   // Version_Spec parsed_vcp_version = {0.0};
   Byte_Value_Array commands = NULL;
   GPtrArray * vcp_features = NULL;
   bool  raw_cmds_segment_seen = false;
   bool  raw_vcp_features_seen = false;

   // Apple Cinema display violates spec, does not surround capabilities string with parens
   if (buf_start[0] == '(') {
      // for now, don't try to fix bad string
      assert(buf_start[buf_len-1] == ')' );

      buf_start = buf_start+1;
      buf_len = buf_len -2;
   }

   while (buf_len > 0) {
      Capabilities_Segment * seg = next_capabilities_segment(buf_start, buf_len);
      buf_start = seg->remainder_start;
      buf_len   = seg->remainder_len;

      if (debug) {
         printf("Segment:  |%.*s| -> |%.*s|\n",
                seg->name_len,  seg->name_start,
                seg->value_len, seg->value_start
               );
      }
      if (memcmp(seg->name_start, "cmds", seg->name_len) == 0) {
         raw_cmds_segment_seen = true;
         commands = parse_cmds_segment(seg->value_start, seg->value_len);
      }
      else if (memcmp(seg->name_start, "vcp", seg->name_len) == 0 ||
               memcmp(seg->name_start, "VCP", seg->name_len) == 0      // hack for Apple Cinema Display
              )
      {
         vcp_features = parse_vcp_segment(seg->value_start, seg->value_len);
         raw_vcp_features_seen = true;
      }
      else if (memcmp(seg->name_start, "mccs_version_string", seg->name_len) == 0) {
         DBGMSF(debug, "MCCS version: %.*s", seg->value_len, seg->value_start);
         // n. pointer will be stored in pcaps
         mccs_ver_string = chars_to_string(seg->value_start, seg->value_len);
      }
      else {
         // additional segment names seen: asset_eep, mpu, mswhql
         DBGMSF(debug, "Ignoring segment: %.*s", seg->name_len, seg->name_start);
      }
      free(seg);
   }

   // n. may be damaged
   Parsed_Capabilities * pcaps
         = new_parsed_capabilities(
              raw_value,
              mccs_ver_string,          // this pointer is saved in returned struct
              raw_cmds_segment_seen,
              raw_vcp_features_seen,
              commands,                 // each stored byte is command id
              vcp_features);

   DBGMSF(debug, "Returning %p", pcaps);
   if (pcaps) {
      DBGMSF(debug, "vcp_features.len = %d", vcp_features->len);
      DBGMSF(debug, "pcaps->vcp_features.len = %d", pcaps->vcp_features->len);
   }
   return pcaps;
}


/** Parses a capabilities string passed in a #Buffer object.
 *
 *  @param  capabilities   pointer to #Buffer
 *
 *  @return pointer to newly allocated #Parsed_Capabilities structure
 */
Parsed_Capabilities* parse_capabilities_buffer(
      Buffer * capabilities)
{
   // dump_buffer(capabilities);
   int len = capabilities->len - 1;
   while (capabilities->bytes[len] == '\0')  {
      // strip trailings 0's - 2 seen
      // printf("%d\n", len);
      len--;
   }
   len++;
   return parse_capabilities((char *)capabilities->bytes, len);
}


/** Parses a capabilities string passed as a character string.
 *
 *  @param  caps   null terminated capabilities string
 *
 *  @return pointer to newly allocated #Parsed_Capabilities structure
 */
Parsed_Capabilities* parse_capabilities_string(
      char * caps)
{
    return parse_capabilities(caps, strlen(caps));
}


/** Returns list of feature ids in a #Parsed_Capabilities structure.
 *
 *  @param pcaps           pointer to #Parsed_Capabilities
 *  @param readable_only   restrict returned list to readable features
 *
 *  @return  #Byte_Bit_Flags indicating features found
 */
Byte_Bit_Flags parsed_capabilities_feature_ids(
      Parsed_Capabilities * pcaps,
      bool                  readable_only)
{
   assert(pcaps);
   bool debug = false;
   DBGMSF(debug, "Starting. readable_only=%s, feature count=%d",
                 bool_repr(readable_only), pcaps->vcp_features->len);

   Byte_Bit_Flags flags = bbf_create();
   if (pcaps->vcp_features) {    // pathological case of 0 length capabilities string
      for (int ndx = 0; ndx < pcaps->vcp_features->len; ndx++) {
         Capabilities_Feature_Record * frec = g_ptr_array_index(pcaps->vcp_features, ndx);
         // DBGMSG("Feature 0x%02x", frec->feature_id);

         bool add_feature_to_list = true;
         if (readable_only) {
            VCP_Feature_Table_Entry * vfte = vcp_find_feature_by_hexid_w_default(frec->feature_id);
            if (!is_feature_readable_by_vcp_version(vfte, pcaps->parsed_mccs_version))
               add_feature_to_list = false;
            if (vfte->vcp_global_flags & DDCA_SYNTHETIC)
               free_synthetic_vcp_entry(vfte);
         }
         if (add_feature_to_list)
            bbf_set(flags, frec->feature_id);
      }
   }

   DBGMSF(debug, "Returning Byte_Bit_Flags: %s", bbf_to_string(flags, NULL, 0));
   return flags;
}


/** Checks if it's possible that a monitor support table reads.
 *
 * Alternatively stated, checks the parsed capabilities to see if table
 * reads can definitely be ruled out.
 *
 * @param   pcaps  pointer to #Parsed_Capabilities (may be null)
 *
 * @return **false** if  **pcaps** is non-null and a commands segment was
 *         parsed and neither Table Read Request nor Table Read Reply was found,
 *         **true** otherwise
 */
bool parsed_capabilities_may_support_table_commands(Parsed_Capabilities * pcaps) {
   bool result = true;
   if (pcaps && pcaps->raw_cmds_segment_seen && pcaps->commands) {
      if ( !bva_contains(pcaps->commands, 0xe2)  &&      // Table Read Request
           !bva_contains(pcaps->commands, 0xe4)          // Table Read Reply
         )
         result = false;
   }
   return result;
}



#ifdef TESTS
//
// Tests
//

void test_segment(char * text) {
   char * start = text;
   int len   = strlen(text);
   Capabilities_Segment * capseg = next_capabilities_segment(start, len);
   free(capseg);      // it's just a test function, but avoid coverity flagging memory leak
}


void test_segments() {
   test_segment("vcp(10 20)");
   test_segment("vcp(10 20)abc");
   test_segment("vcp(10 20 30( asdf ))x");
}


void test_parse_caps() {
   Parsed_Capabilities * pcaps = parse_capabilities_string("(alpha(adsf)vcp(10 20 30(31 32) ))");
   report_parsed_capabilities(pcaps);
   free_parsed_capabilities(pcaps);
}
#endif
