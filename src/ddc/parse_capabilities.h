/*  parse_capabilities.h
 *
 *  Created on: Jul 16, 2014
 *      Author: rock
 *
 *  Parse the capabilities string.
 */

#ifndef PARSE_CAPABILITIES_H_
#define PARSE_CAPABILITIES_H_

#include <glib.h>

#include <util/data_structures.h>

#include <base/ddc_base_defs.h>
#include <base/util.h>


#define PARSED_CAPABILITIES_MARKER "CAPA"
typedef struct {
   char              marker[4];       // always "CAPA"
   char *            raw_value;
   char *            mccs_ver;
   Byte_Value_Array  commands;        // each stored byte is command id
   GArray *          vcp_features;    // entries are VCP_Feature_Record
   Version_Spec      parsed_mccs_version;
} Parsed_Capabilities;


Parsed_Capabilities* parse_capabilities_buffer(Buffer * capabilities);
Parsed_Capabilities* parse_capabilities_string(char * capabilities);
void report_parsed_capabilities(Parsed_Capabilities* pcaps);
void free_parsed_capabilities(Parsed_Capabilities * pcaps);


// Tests

void test_segments();
void test_parse_caps();

#endif /* PARSE_CAPABILITIES_H_ */
