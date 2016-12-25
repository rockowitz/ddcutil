/* hiddev_reports.h
 *
 * <copyright>
 * Copyright (C) 2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef HIDDEV_REPORTS_H_
#define HIDDEV_REPORTS_H_

#include <linux/hiddev.h>
#include <stdbool.h>

void init_hiddev_reports();

void report_hiddev_devinfo(struct hiddev_devinfo * devinfo, bool lookup_names, int depth);
void report_hiddev_device_by_fd(int fd, int depth);

void report_hiddev_usage_ref(struct hiddev_usage_ref * uref, int depth);
void report_hiddev_usage_ref_multi(struct hiddev_usage_ref_multi * uref_multi, int depth);

void report_hiddev_report_info(struct hiddev_report_info * rinfo, int depth);
void report_hiddev_field_info(struct hiddev_field_info * finfo, int depth);

char * hiddev_interpret_report_id(__u32 report_id);

char * hiddev_interpret_usage_code(int usage_code );

#endif /* UTIL_HIDDEV_REPORTS_H_ */
