/** @file hiddev_reports.h
 */

// Copyright (C) 2016-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef HIDDEV_REPORTS_H_
#define HIDDEV_REPORTS_H_

#include <linux/hiddev.h>
#include <stdbool.h>

void init_hiddev_reports();

void dbgrpt_hiddev_devinfo(struct hiddev_devinfo * devinfo, bool lookup_names, int depth);
void dbgrpt_hiddev_device_by_fd(int fd, int depth);

void dbgrpt_hiddev_usage_ref(struct hiddev_usage_ref * uref, int depth);
void dbgrpt_hiddev_usage_ref_multi(struct hiddev_usage_ref_multi * uref_multi, int depth);

void dbgrpt_hiddev_report_info(struct hiddev_report_info * rinfo, int depth);
void dbgrpt_hiddev_field_info(struct hiddev_field_info * finfo, int depth);

char * hiddev_interpret_report_id(__u32 report_id);

char * hiddev_interpret_usage_code(int usage_code );

#endif /* UTIL_HIDDEV_REPORTS_H_ */
