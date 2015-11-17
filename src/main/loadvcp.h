/*  loadvcp.h
 *
 *  Created on: Aug 16, 2014
 *      Author: rock
 *
 *  Load/store VCP settings from/to file.
 */

#ifndef LOADVCP_H_
#define LOADVCP_H_

#include <base/displays.h>


bool loadvcp(const char * fn);

bool dumpvcp(Display_Ref * dref, char * optional_filename);

#endif /* LOADVCP_H_ */
