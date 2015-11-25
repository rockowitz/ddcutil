/*  edid.h
 *
 *  Created on: Oct 17, 2015
 *      Author: rock
 *
 *  Functions for processing the EDID data structure, irrespective of how
 *  the bytes of the EDID are obtained.
 */

#ifndef EDID_H_
#define EDID_H_

#include <stdbool.h>

#include "util/coredefs.h"
#include "base/util.h"


// Field sizes for holding strings extracted from an EDID
// Note that these are sized to allow for a trailing null character.

#define EDID_MFG_ID_FIELD_SIZE        4
#define EDID_MODEL_NAME_FIELD_SIZE   14
#define EDID_SERIAL_ASCII_FIELD_SIZE 14


//Calculates checksum for a 128 byte EDID
Byte edid_checksum(Byte * edid);


void parse_mfg_id_in_buffer(Byte * mfgIdBytes, char * buffer, int bufsize);


// Extracts the 3 character manufacturer id from an EDID byte array.
// The id is returned, with a trailing null character, in a buffer provided by the caller.
void get_edid_mfg_id_in_buffer(Byte* edidbytes, char * result, int bufsize);


/* Extracts the ASCII model name and serial number from an EDID.
 * The maximum length of each of these strings is 13 bytes.
 *
 * Returns:
 *    true if both fields found, false if not
 */
bool get_edid_modelname_and_sn(
        Byte* edidbytes,
        char* namebuf,
        int   namebuf_len,
        char* snbuf,
        int   snbuf_len);


#define EDID_MARKER_NAME "EDID"
typedef
struct {
   char         marker[4];          // always "EDID"
   Byte         bytes[128];
   char         mfg_id[EDID_MFG_ID_FIELD_SIZE];
   char         model_name[EDID_MODEL_NAME_FIELD_SIZE];
   char         serial_ascii[EDID_SERIAL_ASCII_FIELD_SIZE];
   int          year;    // can be year of manufacture or model
   bool         is_model_year;   // if true, year is model year, if false, is manufacture year
   Byte         edid_version_major;
   Byte         edid_version_minor;
} Parsed_Edid;


Parsed_Edid * create_parsed_edid(Byte* edidbytes);
void          report_parsed_edid(Parsed_Edid * edid, bool verbose, int depth);



#endif /* EDID_H_ */
