/*
 * linux_errno.h
 *
 *  Created on: Nov 4, 2015
 *      Author: rock
 */

#ifndef LINUX_ERRNO_H_
#define LINUX_ERRNO_H_

#include "base/status_code_mgt.h"

void init_linux_errno();


// Interpret system error number
// not thread safe, always points to same internal buffer, contents overwritten on each call

char * linux_errno_name(int error_number);
char * linux_errno_desc(int error_number);

char * errno_name(int error_number);

// still used?
char * errno_name_negative(int negative_error_number);

Status_Code_Info * get_errno_info(int errnum);

Status_Code_Info * get_negative_errno_info(int errnum);

#endif /* LINUX_ERRNO_H_ */
