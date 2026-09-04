/** \file dw_suspend_resume.h
 *  Detection of system resume from sleep, combining every available method
 *
 *  Consults the logind PrepareForSleep signals recorded by dbus_util.c
 *  together with the clock comparison in util/suspend_resume_util.h.  The
 *  clock half is separate because it needs no dbus and has a util-layer
 *  consumer; this half is display watch specific and does need dbus.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_SUSPEND_RESUME_H_
#define DW_SUSPEND_RESUME_H_

#include <stdbool.h>
#include <stdint.h>

/** Which of the detection methods in #recently_resumed_from_sleep() answered.
 *  Reported so that a caller's messages can describe the machine's actual
 *  state: inside an open sleep cycle the system need not have slept yet.
 */
typedef enum {
   RESUME_DETECTED_NONE,            ///< no recent resume
   RESUME_DETECTED_BY_DBUS,         ///< logind PrepareForSleep(false) timestamp
   RESUME_DETECTED_BY_CLOCKTIME,    ///< CLOCK_BOOTTIME/CLOCK_MONOTONIC divergence
   RESUME_DETECTED_IN_SLEEP_CYCLE   ///< running inside an open sleep cycle
} Resume_Detection;

const char * resume_detection_description(Resume_Detection detection);
bool         recently_resumed_from_sleep(int                within_ms,
                                         uint64_t *         millisec_since_loc,
                                         Resume_Detection * detection_loc);

#endif /* DW_SUSPEND_RESUME_H_ */
