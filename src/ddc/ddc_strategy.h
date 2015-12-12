/* ddc_strategy.h
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

#ifndef DDC_STRATEGY_H_
#define DDC_STRATEGY_H_

#include "util/coredefs.h"

#include "base/displays.h"
#include "base/status_code_mgt.h"


// For future use

void init_ddc_strategies();


typedef Global_Status_Code (*DDC_Raw_Writer)(Display_Handle * dh, int bytect, Byte * bytes);
typedef Global_Status_Code (*DDC_Raw_Reader)(Display_Handle * dh, int bufsize, Byte * buffer);

typedef struct {
   DDC_IO_Mode     io_mode;
   DDC_Raw_Writer  writer;
   DDC_Raw_Reader  reader;
}  DDC_Strategy;

DDC_Raw_Writer ddc_raw_writer(Display_Handle * dh);
DDC_Raw_Reader ddc_raw_reader(Display_Handle * dh);


#endif /* DDC_STRATEGY_H_ */
