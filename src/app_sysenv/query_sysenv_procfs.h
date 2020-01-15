/** @file query_sysenv_procfs.h
 *
 *  Query environment using /proc file system
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef QUERY_SYSENV_PROCFS_H_
#define QUERY_SYSENV_PROCFS_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

int  query_proc_modules_for_video();
bool query_proc_driver_nvidia();

#endif /* QUERY_SYSENV_PROCFS_H_ */
