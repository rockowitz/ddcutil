/*
 * vcp_feature_codes.h
 *
 *  Created on: Jun 17, 2014
 *      Author: rock
 */

#ifndef VCP_FEATURE_CODES_H_
#define VCP_FEATURE_CODES_H_

#include <util/string_util.h>

#include <base/ddc_packets.h>
#include <base/util.h>

#include <ddc/vcp_feature_code_data.h>


// VCP Feature Table




Format_Feature_Detail_Function get_feature_detail_function( VCP_Feature_Table_Entry * pvft_entry);

Format_Table_Feature_Detail_Function get_table_feature_detail_function(VCP_Feature_Table_Entry * pvft_entry);


#define NULL_VCP_CODE (0x00)     /* used for unrecognized codes */

void list_feature_codes();


VCP_Feature_Table_Entry * find_feature_by_charid(char * id);


VCP_Feature_Table_Entry * create_dummy_feature_for_charid(char * id);

char * get_feature_name(Byte feature_id);


extern int vcp_feature_code_count;    // number of entries in VCP code table

VCP_Feature_Table_Entry * get_vcp_feature_table_entry(int ndx);





typedef enum {SUBSET_SCAN, SUBSET_ALL, SUBSET_SUPPORTED, SUBSET_COLORMGT, SUBSET_PROFILE} VCP_Feature_Subset;





#endif /* VCP_FEATURE_CODES_H_ */
