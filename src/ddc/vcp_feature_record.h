/*  vcp_feature_record.h
 *
 *  Created on: Nov 1, 2015
 *      Author: rock
 */

#ifndef VCP_FEATURE_RECORD_H_
#define VCP_FEATURE_RECORD_H_

#include "util/data_structures.h"

#include "base/util.h"


#define VCP_FEATURE_MARKER "VCPF"
typedef struct {
     char              marker[4];     // always "VCPF"
     Byte              feature_id;
     Byte_Value_Array  values;
     Byte_Bit_Flags    bbflags;       // alternative
     char *            value_string;
} VCP_Feature_Record;

VCP_Feature_Record * new_VCP_Feature_Record(Byte feature_id, char * value_string_start, int value_string_len);
void free_vcp_feature(VCP_Feature_Record * pfeat);
void report_feature(VCP_Feature_Record * vfr, Version_Spec vcp_version);

#endif /* VCP_FEATURE_RECORD_H_ */
