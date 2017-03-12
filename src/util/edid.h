/* edid.h
 *
 * Functions for processing the EDID data structure, irrespective of how
 * the bytes of the EDID are obtained.
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

/** @file edid.h
 * Functions to interpret EDID
 */

#ifndef EDID_H_
#define EDID_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "coredefs.h"


// Field sizes for holding strings extracted from an EDID
// Note that these are sized to allow for a trailing null character.

#define EDID_MFG_ID_FIELD_SIZE        4
#define EDID_EXTRA_STRING_FIELD_SIZE 14
#define EDID_MODEL_NAME_FIELD_SIZE   14
#define EDID_SERIAL_ASCII_FIELD_SIZE 14

//Calculates checksum for a 128 byte EDID
Byte edid_checksum(Byte * edid);

void parse_mfg_id_in_buffer(Byte * mfgIdBytes, char * buffer, int bufsize);

// Extracts the 3 character manufacturer id from an EDID byte array.
// The id is returned, with a trailing null character, in a buffer provided by the caller.
void get_edid_mfg_id_in_buffer(Byte* edidbytes, char * result, int bufsize);


#define EDID_MARKER_NAME "EDID"
/** Represents a parsed EDID */
typedef
struct {
   char         marker[4];                                    ///< always "EDID"
   Byte         bytes[128];                                  ///< raw bytes of EDID
   char         mfg_id[EDID_MFG_ID_FIELD_SIZE];              ///< 3 character mfg id, null terminated
   ushort       model_hex;                                   ///< model hex field
   char         model_name[EDID_MODEL_NAME_FIELD_SIZE];      ///< model name (tag 0xfc)
   uint32_t     serial_binary;                               ///< binary serial number
   char         serial_ascii[EDID_SERIAL_ASCII_FIELD_SIZE];  ///< serial number string (tag 0xff)
   char         extra_descriptor_string[EDID_EXTRA_STRING_FIELD_SIZE];  ///< (tag 0xfe)
   int          year;                    ///< can be year of manufacture or model
   bool         is_model_year;           ///< if true, year is model year, if false, is manufacture year
   Byte         edid_version_major;      ///< EDID major version number
   Byte         edid_version_minor;      ///< EDID minor version number
   ushort       wx;                      ///< whitepoint x coordinate
   ushort       wy;                      ///< whitepoint y coordinate
   ushort       rx;                      ///< red   x coordinate
   ushort       ry;                      ///< red   y coordinate
   ushort       gx;                      ///< green x coordinate
   ushort       gy;                      ///< green y coordinate
   ushort       bx;                      ///< blue  x coordinate
   ushort       by;                      ///< blue  y coordinate
   Byte         video_input_definition;  /// EDID byte 20 (x14)
   // bool         is_digital_input;      // from byte 20 (x14), but 7
   uint8_t      extension_flag;        ///< number of optional extension blocks
   char *       edid_source;           ///< describes source of EDID
} Parsed_Edid;


Parsed_Edid * create_parsed_edid(Byte* edidbytes);
void          report_parsed_edid_base(Parsed_Edid * edid, bool verbose, bool show_raw, int depth);
void          report_parsed_edid(Parsed_Edid * edid, bool verbose, int depth);
void          free_parsed_edid(Parsed_Edid * parsed_edid);

#endif /* EDID_H_ */
