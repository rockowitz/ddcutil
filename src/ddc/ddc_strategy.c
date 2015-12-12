/* ddc_strategy.c
 *
 * Created on: Nov 21, 2015
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2015 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#include <assert.h>

#include "ddc/ddc_strategy.h"



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
