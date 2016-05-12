/* ddc_capabilities.c
 *
 * Parse the capabilities string
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <assert.h>

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "util/debug_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/displays.h"

#include "vcp/ddc_command_codes.h"
#include "vcp/parsed_capabilities_feature.h"

#include "vcp/parse_capabilities.h"


//
// Report parsed data structures
//

void report_commands(Byte_Value_Array cmd_ids) {
   printf("Commands:\n");
   int ct = bva_length(cmd_ids);
   int ndx = 0;
   for (; ndx < ct; ndx++) {
      Byte hval = bva_get(cmd_ids, ndx);
      printf("  Command: %02x (%s)\n", hval, ddc_cmd_code_name(hval));
   }
}


void report_features(GArray* features, Version_Spec vcp_version) {
   printf("VCP Features:\n");
   int ct = features->len;
   int ndx;
   for (ndx=0; ndx < ct; ndx++) {
      Capabilities_Feature_Record * vfr =
          g_array_index(features, Capabilities_Feature_Record *, ndx);
      report_capabilities_feature(vfr, vcp_version);
   }
}


void report_parsed_capabilities(
        Parsed_Capabilities* pcaps,
        MCCS_IO_Mode io_mode)       // needed for proper error message
{
   assert(pcaps && memcmp(pcaps->marker, PARSED_CAPABILITIES_MARKER, 4) == 0);
   // DBGMSG("Starting. pcaps->commands=%p, pcaps->vcp_features=%p", pcaps->commands, pcaps->vcp_features);
   Output_Level output_level = get_output_level();
   if (output_level >= OL_VERBOSE) {
      printf("Unparsed capabilities string: %s\n", pcaps->raw_value);
   }
   bool damaged = false;
   printf("MCCS version: %s\n", (pcaps->mccs_ver) ? pcaps->mccs_ver : "not present");

   if (pcaps->commands)
      report_commands(pcaps->commands);
   else {
      // not an error in the case of USB_IO, as the capabilities string was
      // synthesized and does not include a commands segment
      if (io_mode != USB_IO)
         damaged = true;
   }
   if (pcaps->vcp_features)
      report_features(pcaps->vcp_features, pcaps->parsed_mccs_version);
   else
      damaged = true;
   if (damaged)
      fprintf(stderr, "Capabilities string not completely parsed\n");
}


//
// Lifecycle
//

/* Create a Parsed_Capabilities record.
 *
 * The data structures passed to this function become owned by
 * the newly created Parsed_Capabilties record.
 *
 * Arguments:
 *     ...
 *
 * Returns:
 *    Parsed_Capaibilities record
 */
Parsed_Capabilities * new_parsed_capabilities(
                         char *            raw_value,
                         char *            mccs_ver,
                         Byte_Value_Array  commands,         // each stored byte is command id
                         GArray *          vcp_features
                        )
{
   Parsed_Capabilities* pcaps = calloc(1, sizeof(Parsed_Capabilities));
   memcpy(pcaps->marker, PARSED_CAPABILITIES_MARKER, 4);
   pcaps->raw_value    = raw_value;
   pcaps->mccs_ver     = mccs_ver;
   pcaps->commands     = commands;
   pcaps->vcp_features = vcp_features;

   Version_Spec parsed_vcp_version = {0.0};
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


/* Free a Parsed_Capabilities record
 *
 * Arguments:
 *   pcaps  pointer to Parsed_Capabilities struct
 *
 * Returns:
 *   nothing
 */
void free_parsed_capabilities(Parsed_Capabilities * pcaps) {
   assert( pcaps );
   assert( memcmp(pcaps->marker, PARSED_CAPABILITIES_MARKER, 4) == 0);

   if (pcaps->raw_value)
      free(pcaps->raw_value);

   if (pcaps->mccs_ver)
      free(pcaps->mccs_ver);

   if (pcaps->commands)
      bva_free(pcaps->commands);

   if (pcaps->vcp_features) {
      // DBGMSG("vcp_features->len = %d", pcaps->vcp_features->len);
      int ndx;
      for (ndx=pcaps->vcp_features->len-1; ndx >=0; ndx--) {
         Capabilities_Feature_Record * vfr = g_array_index(pcaps->vcp_features, Capabilities_Feature_Record *, ndx);
         // report_feature(vfr);
         free_capabilities_feature(vfr);
         g_array_remove_index(pcaps->vcp_features, ndx);
      }
      g_array_free(pcaps->vcp_features, true);
   }

   pcaps->marker[3] = 'x';
   free(pcaps);
}



//
// Parsing
//


/* Capabilities string format.

   Parenthesied expression
   containing sequence of "segments"
   each segment consists of a segment name, followed by a parentheized value

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

static
Capabilities_Segment * next_capabilities_segment(char * start, int len) {
   Capabilities_Segment * segment = call_calloc(1, sizeof(Capabilities_Segment), "next_capabilities_segment");
   // bool ok = true;

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
//  or pass a preallocted instance
Byte_Value_Array parse_cmds_segment(char * start, int len) {
   bool debug = false;
   if (debug)
      DBGMSG("Starting. start=%p, len=%d", start, len);

   Byte_Value_Array cmd_ids2 = bva_create();
   bool ok = store_bytehex_list(start, len, cmd_ids2, bva_appender);
   if (!ok) {
      fprintf(stderr, "Error processing commands list: %.*s\n", len, start);
   }
   // report_id_array(cmd_ids, "Command ids found:");
   if (debug) {
      // report_cmd_array(cmd_ids);
      DBGMSG("store_bytehex_list returned %d", ok);
      report_commands(cmd_ids2);
   }
   Byte_Value_Array result = (ok) ? cmd_ids2 : NULL;
   if (debug)
      DBGMSG("returning %p", result);
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

static
char * find_closing_paren(char * start, char * end) {
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
 *    GArray of VCP_Feature_Records
 */
GArray * parse_vcp_segment(char * start, int len) {
   bool debug = false;
   GArray* vcp_array = g_array_sized_new(
              true,             // zero terminated
              true,             // clear all bits on allocation
              sizeof(void *),   // element size
              40);              // initial size
   // Vcp_Code_Table_Entry * vcp_entry;    // future?
   char * pos = start;
   char * end = start + len;
   Byte   cur_feature_id;
   bool   valid_feature;
   int    value_len = 0;      // initialization logically unnecessary, but o.w. get warning
   char * value_start = NULL; // ditto
   while (pos < end) {
      valid_feature = false;
      while(*pos == ' ' && pos < end) pos++;
      if (pos == end)
         break;
      char * st = pos;
      while (*pos != ' ' && *pos != '(' && pos < end) pos++;
      int ln = pos-st;
      // printf("Found: Feature: %.*s\n", ln, st);
      if (ln == 2) {
         cur_feature_id = hhc_to_byte(st);
         valid_feature = true;
         value_start = NULL;
         value_len   = 0;
      }
      else {
         printf("Feature: %.*s (invalid code)\n", ln, st);
      }

      if (*pos == '(') {
         // find matching )
         char * value_end = find_closing_paren(pos, end);
         if (value_end == end) {
            DBGMSG("Value parse terminated without closing paren   " );
            // bad data, what to do?
            exit(1);
         }
         value_start = pos+1;
         value_len = value_end - (pos + 1);
         // printf("  Values: %.*s\n", value_len, value_start);
         pos = value_end + 1;   // point to character after closing paren
      }

      if (valid_feature) {
         Capabilities_Feature_Record * vfr = new_Capabilities_Feature(cur_feature_id, value_start, value_len);
         if (debug) {
            Version_Spec dummy_version = {0,0};
            report_capabilities_feature(vfr, dummy_version);
         }
         g_array_append_val(vcp_array, vfr);
      }
   }
   return vcp_array;
}


/* Parse the entire capabilities string
 *
 * Arguments:
 *    buf_start   starting address of string
 *    buf_len     length of string (not including trailing null)
 *
 * Returns:
 *   pointer to newly allocated ParsedCapabilities structure
 */
Parsed_Capabilities * parse_capabilities(char * buf_start, int buf_len) {
   bool debug = false;
   if (debug) {
      DBGMSG("Starting. len=%d", buf_len);
      hex_dump((Byte*)buf_start, buf_len);
      DBGMSG("Starting. buf_start -> |%.*s|", buf_len, buf_start);
   }

   // Make a copy of the unparsed value
   char * raw_value = chars_to_string(buf_start, buf_len);

   char * mccs_ver  = NULL;
   // Version_Spec parsed_vcp_version = {0.0};
   Byte_Value_Array commands = NULL;
   GArray * vcp_features = NULL;

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
         commands = parse_cmds_segment(seg->value_start, seg->value_len);
      }
      else if (memcmp(seg->name_start, "vcp", seg->name_len) == 0 ||
               memcmp(seg->name_start, "VCP", seg->name_len) == 0      // hack for Apple Cinema Display
              )
      {
         vcp_features = parse_vcp_segment(seg->value_start, seg->value_len);
      }
      else if (memcmp(seg->name_start, "mccs_ver", seg->name_len) == 0) {
         if (debug)
            DBGMSG("MCCS version: %.*s", seg->value_len, seg->value_start);
         mccs_ver = chars_to_string(seg->value_start, seg->value_len);
      }
      else {
         // additional segment names seen: asset_eep, mpu, mswhql
         if (debug)
            DBGMSG("Ignoring segment: %.*s", seg->name_len, seg->name_start);
      }
   }

   // n. may be damaged
   Parsed_Capabilities * pcaps = NULL;
   pcaps = new_parsed_capabilities(
              raw_value,
              mccs_ver,
              commands,         // each stored byte is command id
              vcp_features);

   if (debug)
      DBGMSG("Returning %p", pcaps);
   return pcaps;
}


/* Parse a capabilities string passed in a Buffer object.
 *
 * Arguments:
 *    capabilities   pointer to Buffer
 *
 * Returns:
 *   pointer to newly allocated ParsedCapabilities structure
 */
Parsed_Capabilities* parse_capabilities_buffer(Buffer * capabilities) {
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


/* Parse a capabilities string passed as a character string.
 *
 * Arguments:
 *    caps     null terminated capabilities string
 *
 * Returns:
 *   pointer to newly allocated ParsedCapabilities structure
 */
Parsed_Capabilities* parse_capabilities_string(char * caps) {
    return parse_capabilities(caps, strlen(caps));
}


//
// Tests
//

void test_segment(char * text) {
   char * start = text;
   int len   = strlen(text);
   next_capabilities_segment(start, len);
}


void test_segments() {
   test_segment("vcp(10 20)");
   test_segment("vcp(10 20)abc");
   test_segment("vcp(10 20 30( asdf ))x");
}


void test_parse_caps() {
   parse_capabilities_string("(alpha(adsf)vcp(10 20 30(31 32) ))");
}
