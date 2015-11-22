/*
 * ddc_strategy.c
 *
 *  Created on: Nov 21, 2015
 *      Author: rock
 */

#include <assert.h>

#include <ddc/ddc_strategy.h>



// keep in sync w DDC_IO_Mode
DDC_Strategy ddc_strategies[] = {
      {DDC_IO_DEVI2C, NULL, NULL },
      {DDC_IO_ADL,    NULL, NULL }
};

void validate_ddc_strategies() {
   assert(  ddc_strategies[DDC_IO_DEVI2C].io_mode == DDC_IO_DEVI2C);
   assert(  ddc_strategies[DDC_IO_ADL].io_mode == DDC_IO_ADL);
}



DDC_Raw_Writer ddc_raw_writer(Display_Handle * dh) {
   return ddc_strategies[dh->ddc_io_mode].writer;
}
DDC_Raw_Reader ddc_raw_reader(Display_Handle * dh) {
   return ddc_strategies[dh->ddc_io_mode].reader;
}


void init_ddc_strategies() {
   validate_ddc_strategies();
}
