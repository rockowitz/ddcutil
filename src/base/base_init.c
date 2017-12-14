/** @file base_init.c
 * Master base services initialization
 */

/*
 * <copyright>
 * Copyright (C) 2014-2017 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "core.h"
#include "ddc_packets.h"
#include "error_info.h"
#include "execution_stats.h"
#include "linux_errno.h"
#include "sleep.h"

#include "base_init.h"



/** Master initialization function for files in subdirectory base
 */
void init_base_services() {
   init_msg_control();
   errinfo_init(psc_name, psc_desc);
   init_sleep_stats();
   init_execution_stats();
   init_status_code_mgt();
   // init_linux_errno();
}
