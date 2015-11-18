/*
 * ddc_base_defs.h
 *
 *  Created on: Nov 17, 2015
 *      Author: rock
 */

#ifndef DDC_BASE_DEFS_H_
#define DDC_BASE_DEFS_H_

#include <util/coredefs.h>

// Not logically a part of packet definitions, but it needs to be in directory base, not ddc
// to avoid circular includes
typedef struct {
    Byte  major;
    Byte  minor;
} Version_Spec;

typedef enum {I2C_IO_STRATEGY_FILEIO, I2C_IO_STRATEGY_IOCTL} I2C_IO_Strategy_Id;

#endif /* DDC_BASE_DEFS_H_ */
