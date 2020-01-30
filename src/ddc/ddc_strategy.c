/** @file ddc_strategy.c
 */

// Copyright (C) 2015-2017 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <assert.h>
/** \endcond */

#include "ddc/ddc_strategy.h"

// keep in sync w DDC_IO_Mode
DDC_Strategy ddc_strategies[] = {
      {DDCA_IO_I2C, NULL, NULL },
      {DDCA_IO_ADL,    NULL, NULL },
      {DDCA_IO_USB,        NULL, NULL }
};

void validate_ddc_strategies() {
   assert(  ddc_strategies[DDCA_IO_I2C].io_mode == DDCA_IO_I2C);
   assert(  ddc_strategies[DDCA_IO_ADL].io_mode    == DDCA_IO_ADL);
   assert(  ddc_strategies[DDCA_IO_USB].io_mode    == DDCA_IO_USB);
}

DDC_Raw_Writer ddc_raw_writer(Display_Handle * dh) {
   assert(dh && dh->dref);
   return ddc_strategies[dh->dref->io_path.io_mode].writer;
}

DDC_Raw_Reader ddc_raw_reader(Display_Handle * dh) {
   assert(dh && dh->dref);
   return ddc_strategies[dh->dref->io_path.io_mode].reader;
}

void init_ddc_strategies() {
   validate_ddc_strategies();
}
