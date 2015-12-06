/*  loadvcp.h
 *
 *  Created on: Aug 16, 2014
 *      Author: rock
 *
 *  Load/store VCP settings from/to file.
 */

#ifndef LOADVCP_H_
#define LOADVCP_H_

#include <base/status_code_mgt.h>
#include <base/displays.h>

bool loadvcp_from_file(const char * fn);
bool dumpvcp(Display_Ref * dref, char * optional_filename);
char * dumpvcp_to_string_by_display_handle(Display_Handle * dh);
char * dumpvcp_to_string_by_display_ref(Display_Ref * dref);
Global_Status_Code loadvcp_from_string(char * catenated);

#endif /* LOADVCP_H_ */
