/** \file build_info.h
 *
 *  Build Information: version, compilation options, etc.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef BUILD_INFO_H_
#define BUILD_INFO_H_

extern const char * BUILD_VERSION;

// extern const char * BUILD_DATE;    // unimplemented
// extern const char * BUILD_TIME;    // unimplemented

void report_build_options(int depth);

#endif /* BUILD_INFO_H_ */
