/*
 * adl_errors.h
 *
 *  Created on: Nov 3, 2015
 *      Author: rock
 */

#ifndef ADL_ERRORS_H_
#define ADL_ERRORS_H_

// #include <base/util.h>

#include "base/status_code_mgt.h"

void init_adl_errors() ;

Status_Code_Info * get_adl_status_description(int errnum);

#endif /* ADL_ERRORS_H_ */
