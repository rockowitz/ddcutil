/** \file build_timestamp.c
 *
 *  Timestamp generated at each build.
 */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "base/build_details.h"       // created by Makefile
#include "base/build_timestamp.h"

const char * BUILD_DATE = __DATE__;
const char * BUILD_TIME = __TIME__;
