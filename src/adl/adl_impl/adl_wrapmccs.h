/*
 * adl_wrapmccs.h
 *
 *  Created on: Oct 18, 2015
 *      Author: rock
 *
 *  File mccs.h from the ADL SDK lacks ifdef tests, which
 *  can cause double inclusion resulting in errors.  This
 *  file should be included wherever mccs.h is needed.
 */

#ifndef ADL_WRAPMCCS_H_
#define ADL_WRAPMCCS_H_

typedef void * HMODULE;    // needed by mccs.h
#include <mccs.h>

#endif /* ADL_WRAPMCCS_H_ */
