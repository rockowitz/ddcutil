/** \file build_timestamp.c
 *
 *  Timestamp generated at each build.
 */

// Copyright (C) 2024 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "base/build_details.h"       // created by Makefile
#include "base/build_timestamp.h"

#ifdef BUILD_TIMESTAMP
const char * BUILD_DATE = __DATE__;
const char * BUILD_TIME = __TIME__;
#else
const char * BUILD_DATE = "Not set";
const char * BUILD_TIME = "Not set";
#endif

