/** \file suspend_resume_util.h
 *  Detection of system suspend and resume
 *
 *  Concerns the sleep states the **system** enters, i.e. suspend to RAM or to
 *  idle, and in particular when this process last resumed from one.  Not to be
 *  confused with the delays ddcutil itself takes, which are base/sleep.h and
 *  base/tuned_sleep.h.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SUSPEND_RESUME_UTIL_H_
#define SUSPEND_RESUME_UTIL_H_

#include <stdbool.h>
#include <stdint.h>

void       init_accumulated_sleep();
bool       recently_resumed_from_sleep_by_clocktime0(bool no_mutate, bool * detected_now_loc);
bool       recently_resumed_from_sleep_by_clocktime(bool * detected_now_loc);
uint64_t   millisec_since_resume_detected_by_clocktime();

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

#endif /* SUSPEND_RESUME_UTIL_H_ */
