// app_sysenv_services.c

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "app_sysenv_services.h"
#include "query_sysenv_detailed_bus_pci_devices.h"
#include "query_sysenv_sysfs.h"
 
void init_app_sysenv_services() {
   init_query_detailed_bus_pci_devices();
   init_query_sysenv_sysfs();
   init_query_sysenv();
}

