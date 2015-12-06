/*  ddc_capabilities.h
 *
 *  Created on: Jun 11, 2014
 *      Author: rock
 */

#ifndef DDC_CAPABILITIES_H_
#define DDC_CAPABILITIES_H_

#include <stdbool.h>

#include "base/status_code_mgt.h"
#include "base/util.h"


// Statistics
void ddc_reset_multi_part_read_stats();
void ddc_report_multi_part_read_stats();

// Retry management
void ddc_set_max_multi_part_read_tries(int ct);
int ddc_get_max_multi_part_read_tries();


Global_Status_Code multi_part_read_with_retry(
                      Display_Handle * dh,
                      Byte          request_type,
                      Byte          request_subtype,   // VCP feature code for table read, ignore for capabilities
                      Buffer**      ppbuffer);

#endif /* DDC_CAPABILITIES_H_ */
