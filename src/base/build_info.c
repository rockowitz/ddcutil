/** \file build_info.c
 *
 *  Build Information: version, build options etc.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config.h>

#include "util/report_util.h"
#include "base/build_info.h"

const char * BUILD_VERSION = VERSION;      /**< ddcutil version */

// TODO: patch dummy values at link time
// const char * BUILD_DATE = __DATE__;
const char * BUILD_DATE = "mmm dd yyyy";   /**<  build date, to be patched at link time */
// const char * BUILD_TIME = __TIME__;
const char * BUILD_TIME = "hh:mm:ss";      /**< build time, to be patched at link time */


void report_build_options(int depth) {
   int d1 = depth+1;
   rpt_label(depth, "General Build Options:");

// doesn't work, fails if option name undefined
// #define REPORT_BUILD_OPTION(_name) rpt_vstring(d1, "%-20s %s", #_name ":", ( _name ## "+0" ) ? "Set" : "Not Set" )

#define IS_SET(_name) \
      rpt_vstring(d1, "%-20s Defined", #_name ":");
#define NOT_SET(_name) \
      rpt_vstring(d1, "%-20s Not defined", #_name ":");

#ifdef BUILD_SHARED_LIB
   IS_SET(BUILD_SHARED_LIB);
#else
   NOT_SET(BUILD_SHARED_LIB);
#endif

#ifdef ENABLE_ENVCMDS
   IS_SET(ENABLE_ENVCMDS);
#else
   NOT_SET(ENABLE_ENVCMDS);
#endif

#ifdef ENABLE_FAILSIM
   IS_SET(ENABLE_FAILSIM);
#else
   NOT_SET(ENABLE_FAILSIM);
#endif

#ifdef ENABLE_UDEV
   IS_SET(ENABLE_UDEV);
#else
   NOT_SET(ENABLE_UDEV);
#endif

#ifdef USE_X11
   IS_SET(USE_X11);
#else
   NOT_SET(USE_X11);
#endif

#ifdef USE_LIBDRM
   IS_SET(USE_LIBDRM);
#else
   NOT_SET(USE_LIBDRM);
#endif

#ifdef USE_USB
   IS_SET(USE_USB);
#else
   NOT_SET(USE_USB);
#endif

#ifdef WITH_ASAN
   IS_SET(WITH_ASAN);
#else
   NOT_SET(WITH_ASAN);
#endif

   rpt_nl();

   rpt_label(depth, "Private Build Options:");

#ifdef TARGET_LINUX
   IS_SET(TARGET_LINUX);
#else
   NOT_SET(TARGET_LINUX);
#endif

#ifdef TARGET_BSD
   IS_SET(TARGET_BSD);
#else
   NOT_SET(TARGET_BSD);
#endif

#ifdef INCLUDE_TESTCASES
   IS_SET(INCLUDE_TESTCASES);
#else
   NOT_SET(INCLUDE_TESTCASES);
#endif

   rpt_nl();
}

