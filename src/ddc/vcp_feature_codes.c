/*
 * vcp_feature_codes.c
 *
 *  Created on: Jun 17, 2014
 *      Author: rock
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <util/string_util.h>


#include <ddc/vcp_feature_code_data.h>
#include <ddc/vcp_feature_codes.h>


// typedef char * (*GetNameFunction)(Byte id);


//
// Functions that return a VCP_Feature_Table_Entry
//

/* Creates a dummy VCP feature table entry for a feature code,
 * based on a hex string representation of the code.
 * It is the responsibility of the caller to free this memory.
 *
 * Arguments:
 *    id     feature id, as character string
 *
 * Returns:
 *   created VCP_Feature_Table_Entry
 *   NULL if id does not consist of 2 hex characters
 */
VCP_Feature_Table_Entry * create_dummy_feature_for_charid(char * id) {
   VCP_Feature_Table_Entry * result = NULL;
   Byte hexId;
   bool ok = hhs_to_byte_in_buf(id, &hexId);
   if (!ok) {
      printf("(%s) Invalid feature code: %s\n", __func__, id);
   }
   else {
      result = create_dummy_feature_for_hexid(hexId);
   }
   // printf("(%s) Returning %p\n", __func__, result);
   return result;
}


/* Returns an entry in the VCP feature table based on its index in the table.
 *
 * Arguments:
 *    ndx     table index
 *
 * Returns:
 *    VCP_Feature_Table_Entry
 */
VCP_Feature_Table_Entry * get_vcp_feature_table_entry(int ndx) {
   // printf("(%s) ndx=%d, vcp_code_count=%d  \n", __func__, ndx, vcp_code_count );
   assert( 0 <= ndx && ndx < vcp_feature_code_count);
   return &vcp_code_table[ndx];
}



/* Returns an entry in the VCP feature table based on the character
 * string representation of its feature code.
 *
 * Arguments:
 *    id    feature id
 *
 * Returns:
 *    VCP_Feature_Table_Entry
 *    NULL if id does not consist of 2 hex characters, or feature code not found
 */
VCP_Feature_Table_Entry * find_feature_by_charid(char * id) {
   bool debug = false;
   if (debug)
      printf("(%s) Starting id=|%s|  \n", __func__, id );
   VCP_Feature_Table_Entry * result = NULL;

   Byte hexId;
   bool ok = hhs_to_byte_in_buf(id, &hexId);
   if (!ok) {
      if (debug)
         printf("(%s) Invalid feature code: %s\n", __func__, id);
   }
   else {
      result = find_feature_by_hexid(hexId);
   }
   if (debug)
      printf("(%s) Returning %p\n", __func__, result);
   return result;
}


// Functions that lookup a value contained in a VCP_Feature_Table_Entry,
// returning a default if the value is not set for that entry.

Format_Feature_Detail_Function get_feature_detail_function( VCP_Feature_Table_Entry * pvft_entry) {
   assert(pvft_entry != NULL);

   // TODO:
   // if VCP_V2NC_V3T, then get version id
   // based on version id, choose .formatter or .formatter_v3
   // NO - test needs to be set in caller, this must return a Format_Feature_Detail_Function, which is not for Table

   Format_Feature_Detail_Function func = pvft_entry->formatter;
   if (!func)
      func = format_feature_detail_debug_continuous;
   return func;
}


char * get_feature_name(Byte feature_id) {
   char * result = NULL;
   VCP_Feature_Table_Entry * vcp_entry = find_feature_by_hexid(feature_id);
   if (vcp_entry)
      result = vcp_entry->name;
   else if (0xe0 <= feature_id && feature_id <= 0xff)
      result = "manufacturer specific feature";
   else
      result = "unrecognized feature";
   return result;
}


bool default_table_feature_detail_function(Version_Spec vcp_version, Buffer * data, Buffer** presult) {
         printf("(%s) vcp_version=%d.%d\n", __func__, vcp_version.major, vcp_version.minor);
         int hexbufsize = buffer_length(data) * 3;
         Buffer * outbuf = buffer_new(hexbufsize, "default_table_feature_detail_function");

         char space = ' ';
         hexstring2(data->bytes, data->len, &space, false /* upper case */, (char *) outbuf->bytes, hexbufsize);
         return outbuf;
}

Format_Table_Feature_Detail_Function get_table_feature_detail_function( VCP_Feature_Table_Entry * pvft_entry) {
   assert(pvft_entry != NULL);

   // TODO:
   // if VCP_V2NC_V3T, then get version id
   // based on version id, choose .formatter or .formatter_v3
   // NO - test needs to be set in caller, this must return a Format_Feature_Detail_Function, which is not for Table

   Format_Table_Feature_Detail_Function func = pvft_entry->table_formatter;
   if (!func)
      func = default_table_feature_detail_function;
   return func;
}



#ifdef REFERENCE
         char * hexstring2(
                   const unsigned char * bytes,      // bytes to convert
                   int                   len,        // number of bytes
                   const char *          sep,        // separator character (used how?)
                   bool                  uppercase,  // use upper case hex characters?
                   char *                buffer,     // buffer in which to return hex string
                   int                   bufsz);     // buffer size
#endif





#ifdef OLD
Format_Table_Feature_Detail_Function get_table_feature_detail_function(Byte feature_id) {
   // TODO: add lookup by feature id
   return default_table_feature_detail_function;

}
#endif


//
// VCP_Feature_Table Use
//


// Creates humanly readable interpretation of VCP feature flags.
//
// The result is returned in a buffer supplied by the caller.

char * interpret_vcp_flags(VCP_Feature_Flags flags, char* buf, int buflen) {
   // printf("(%s) flags: 0x%04x\n", __func__, flags);
   char * rwmsg = "";
   if (flags & VCP_RO)
      rwmsg = "ReadOnly ";
   else if (flags & VCP_WO)
      rwmsg = "WriteOnly";
   else if (flags & VCP_RW)
      rwmsg = "ReadWrite";

   char * typemsg = "";
   // NEED TO ALSO HANDLE TABLE TYPE
   if (flags & VCP_CONTINUOUS)
      typemsg = "Continuous";
   else if (flags & VCP_NON_CONT)
      typemsg = "Non-continuous";
   else if (flags & VCP_TABLE)
      typemsg = "Table";
   else if (flags & VCP_TYPE_V2NC_V3T)
      typemsg = "V2:NC, V3:Table";
   else
      typemsg = "Type not set";

   char * vermsg = "";
   if (flags & VCP_FUNC_VER)
      vermsg = " (Version specific interpretation)";

   snprintf(buf, buflen, "%s  %s%s", rwmsg, typemsg, vermsg);
   return buf;
}


void list_feature_codes() {
   printf("Recognized VCP feature codes:\n");
   char buf[200];
   int ndx = 0;
   for (;ndx < vcp_feature_code_count; ndx++) {
      VCP_Feature_Table_Entry entry = vcp_code_table[ndx];
      // printf("(%s) code=0x%02x, flags: 0x%04x\n", __func__, entry.code, entry.flags);
      printf("  %02x - %-40s  %s\n",
             entry.code,
             entry.name,
             interpret_vcp_flags(entry.flags, buf, 200)
            );
   }
}

