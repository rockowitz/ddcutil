/*
 * adl_sdk_includes.h
 *
 *  Created on: Oct 18, 2015
 *      Author: rock
 *
 *  Includes all header files from the ADL SDK, setting defines appropriately
 */

#ifndef ADL_SDK_INCLUDES_H_
#define ADL_SDK_INCLUDES_H_

#include <stdlib.h>     // wchar_t, needed by adl_structures.h

#include <adl_sdk/adl_defines.h>
#define LINUX
#include <adl_sdk/adl_structures.h>
#include <adl_sdk/adl_sdk.h>
#include <adl/adl_wrapmccs.h>

#endif /* ADL_SDK_INCLUDES_H_ */
