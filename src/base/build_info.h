/** \file build_info.h
 *
 *  Build Information: version, compilation options, etc.
 */

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef BUILD_INFO_H_
#define BUILD_INFO_H_

// extern const char * BUILD_VERSION;

const char * get_base_ddcutil_version();
const char * get_ddcutil_version_suffix();
const char * get_full_ddcutil_version();

void report_build_options(int depth);

#endif /* BUILD_INFO_H_ */
