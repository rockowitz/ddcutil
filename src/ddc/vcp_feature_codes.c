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
