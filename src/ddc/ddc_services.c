/* ddc_services.c
 *
 * Created on: Jan 2, 2016
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <stdio.h>
#include <vcp/vcp_feature_codes.h>
#include "base/base_services.h"
#include "base/parms.h"

#include "i2c/i2c_do_io.h"

#include "adl/adl_errors.h"
#include "adl/adl_shim.h"

#include "ddc/ddc_multi_part_io.h"
#include "ddc/ddc_packet_io.h"
#include "ddc/ddc_services.h"


/* Master initialization function
 */
void init_ddc_services() {
   // DBGMSG("Executing");

   // base:
   init_base_services();

   // i2c:
   i2c_set_io_strategy(DEFAULT_I2C_IO_STRATEGY);

   // adl:
   init_adl_errors();
   // adl_debug = true;      // turn on adl initialization tracing
   adlshim_initialize();

   // ddc:
   ddc_reset_write_only_stats();
   ddc_reset_write_read_stats();
   ddc_reset_multi_part_read_stats();
   init_vcp_feature_codes();
   init_ddc_packets();   // 11/2015: does nothing
}

// Located here because this function doesn't really belong anywhere else.
void ddc_show_max_tries(FILE * fh) {
   fprintf(fh, "Maximum Try Settings:\n");
   fprintf(fh, "Operation Type             Current  Default\n");
   fprintf(fh, "Write only exchange tries: %8d %8d\n",
               ddc_get_max_write_only_exchange_tries(),
               MAX_WRITE_ONLY_EXCHANGE_TRIES);
   fprintf(fh, "Write read exchange tries: %8d %8d\n",
               ddc_get_max_write_read_exchange_tries(),
               MAX_WRITE_READ_EXCHANGE_TRIES);
   fprintf(fh, "Multi-part exchange tries: %8d %8d\n",
               ddc_get_max_multi_part_read_tries(),
               MAX_MULTI_EXCHANGE_TRIES);
}
