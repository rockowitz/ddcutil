/** @file suspend_resume_util.c
 *  Detection of system resume from sleep by clock comparison
 *
 *  Measures cumulative system sleep as the divergence between CLOCK_BOOTTIME,
 *  which advances while suspended, and CLOCK_MONOTONIC, which does not.
 *  The detector that combines this with the logind PrepareForSleep signals
 *  is in dw/dw_suspend_resume.c.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <syslog.h>
#include "debug_util.h"
#include "report_util.h"      // for tag_output, used by SIMPLE_STD_FUNC_SYSLOG
#include "string_util.h"
#include "syslog_util.h"
#include "timestamp.h"
#include "suspend_resume_util.h"
//
// Detect resume from sleep using CLOCK_BOOTTIME and CLOCK_MONOTONIC.
//
// Detects if resume from sleep has occurred by detecting changes in the
// accumulated sleep time, by using CLOCK_BOOTTIME and CLOCK_MONOTONIC.
// BOOTTIME advances during sleep; MONOTONIC does not.
// Their difference = cumulative time spent asleep since the algorithm was
// Started. An increase in the cumulative time asleep since vs the previous
// value indicates a resume occurred.
//
// This is a cruder mechanism than watching for dbus PrepareForSleep
// signals, but does not require dbus.  It also has the advantage that
// this algorithm will indicate that a sleep has occurred if immediately
// executed after resume, whereas there is a sliver of time between
// when execution resumed and the arrival of the of dbus message.

/** Global baseline set at startup by #init_baseline_accumulated_sleep_ns().
 *  UINT64_MAX means not yet initialized.
 */
static uint64_t global_initial_accumulated_sleep_ns = UINT64_MAX;

/** Per-thread baseline, initialized from the global on each thread's first call.
 *  UINT64_MAX means this thread has not yet initialized its baseline.
 */
static _Thread_local uint64_t previous_accumulated_sleep_ns = UINT64_MAX;

/** CLOCK_BOOTTIME ms at which this thread most recently DETECTED a resume,
 * i.e. the start of the 5-sec "recently resumed" grace window, not its end.
 * The window runs until this value plus 5 sec, and
 * millisec_since_resume_detected_by_clocktime() measures from it.
 * See recently_resumed_from_sleep_by_clocktime().
 *
 * UINT64_MAX, not 0, means no resume has yet been detected on this thread.
 * 0 is a valid CLOCK_BOOTTIME value, and using it as the sentinel put every
 * thread inside the grace window for the first 5 seconds after boot.
 */
static _Thread_local uint64_t most_recent_detection_ms = UINT64_MAX;


/** Gets the current accumulated sleep time.
 *
 *  @return accumulated sleep time in nanosec, 0 if none or if it cannot be read
 *
 *  @remark
 *  The two clocks are sampled sequentially, so the second read is the later of
 *  the two by the interval between them.  With no accumulated sleep the true
 *  difference is ~0, and that interval alone can make it negative.  Computed
 *  unsigned, as it was, such a value wrapped to nearly 2**64: this machine
 *  reported 18446744073709551599, i.e. -17ns.  That is stable while the offset
 *  stays on one side of zero, but a system whose offset jitters across zero
 *  produces one sample small and positive and the next wrapped, and
 *  recently_resumed_from_sleep_by_clocktime0() reads the step between them as
 *  a 584-year increase in accumulated sleep -- a resume that never happened.
 *  Observed as a unit test failure on an aarch64 build worker, which, never
 *  having suspended, sat at an offset of ~0.
 *
 *  So: sample MONOTONIC first, so BOOTTIME is not the earlier of the two, and
 *  compare signed, clamping at 0.  Zero accumulated sleep now reads as 0.
 */
static uint64_t get_accumulated_sleep_ns() {
   struct timespec bt = {0};
   struct timespec mt = {0};
   // Unchecked, a failure leaves the timespec uninitialized and the difference
   // below is stack garbage.
   if (clock_gettime(CLOCK_MONOTONIC, &mt) != 0 ||   // does not advance during sleep
       clock_gettime(CLOCK_BOOTTIME,  &bt) != 0)     // advances during sleep
      return 0;
   int64_t boottime_ns = SECS2NANOS(bt.tv_sec) + bt.tv_nsec;
   int64_t mono_ns     = SECS2NANOS(mt.tv_sec) + mt.tv_nsec;
   int64_t accumulated_sleep_ns = boottime_ns - mono_ns;
   return (accumulated_sleep_ns > 0) ? (uint64_t) accumulated_sleep_ns : 0;
}


/** Records the current accumulated sleep time as the global baseline.
 *
 *  Must be called once at program startup before any additional threads are
 *  created. Each thread's per-thread baseline is seeded from this value on its
 *  first call to #recently_resumed_from_sleep(), so all threads can detect
 *  resumes that occur after this baseline was recorded.
 */
void init_accumulated_sleep() {
   global_initial_accumulated_sleep_ns = get_accumulated_sleep_ns();
}

#ifdef UNUSED
void reset_recently_resumed_by_clocktime_cache() {
   most_recent_detection_ms = UINT64_MAX;   // i.e. no resume yet detected
}
#endif


/** Detects whether the system has resumed from sleep.
 *
 *  Uses the difference between CLOCK_BOOTTIME (advances during sleep) and
 *  CLOCK_MONOTONIC (does not advance during sleep) to measure cumulative
 *  sleep time.  An increase since the prior call on this thread indicates
 *  that a resume occurred.
 *
 *  Once a resume is detected, calls on the same thread within the following
 *  5 seconds (the grace window) also return true, so that multiple call
 *  sites on a thread can each observe the resume.  See the discussion of
 *  the tradeoffs in the function body.
 *
 *  @param  detected_now_loc  if non-NULL, set to true if THIS call detected
 *          the resume, false if it answered from the grace window or found
 *          no resume.
 *  @param  no_mutate  if true, answer without touching any state: the
 *          detection is neither consumed nor recorded, the grace window is
 *          neither opened nor extended, and nothing is written to the system
 *          log.  For an observer, such as a diagnostic report, that must not
 *          alter what a subsequent real caller will see.  Note that a
 *          detection observed this way is still pending: the next call
 *          without no_mutate will report it again, and will be the one to
 *          consume it.
 *  @return true if a resume from sleep was detected on this call, or was
 *          detected on this thread within the past 5 seconds
 *
 *  @remark
 *  Re **detected_now_loc**:  A caller weighing this detector against another
 *  source needs the distinction: a detection on this call means sleep
 *  accumulated that this thread had not yet accounted for, so another
 *  source may not have processed the corresponding event either.
 *  Within the grace window that is no longer true, the resume having been
 *  observed at least once already.
 */
bool recently_resumed_from_sleep_by_clocktime0(bool no_mutate, bool * detected_now_loc) {
   bool debug = false;
   bool resumed = false;
   bool detected_now = false;

   uint64_t cur_boottime_ms = NANOS2MILLIS( cur_boot_time_nanosec());

   // Grace window vs one-shot detection.
   //
   // The delta check in the else branch below is inherently one-shot: it
   // updates previous_accumulated_sleep_ns the moment it detects a resume,
   // so on the very next call the increase is ~0 and it reports no resume.
   // This branch converts that single observation into a state: for 5
   // seconds after a detection, every call on this thread reports
   // "recently resumed".  most_recent_detection_ms does not reset the
   // detector's baseline (previous_accumulated_sleep_ns does that); it
   // records when the window opened.
   //
   // The tradeoff.  One-shot, i.e. this function without this branch, is
   // the simpler contract, with no window length to choose, but the
   // observation is consumed by whichever caller asks first, so multiple
   // call sites on one thread cannot coexist.  Concretely: the priming
   // call in dw_udev_watch(), made before the post-add-event sleep
   // precisely so that its reference point precedes that sleep, would
   // swallow the detection, and the call to
   // dw_pause_if_recently_resumed_from_sleep() on the next iteration of
   // the watch loop would never see it and never pause.  (Not the same
   // iteration: an add event makes that iteration's guard false.)  Both
   // run on the watch thread, and the detector's state is per thread.

   // With the window, "recently resumed" is a state that any number of
   // callers can query for a bounded period. The cost is that true is
   // returned for the whole window, so a caller acting on the bare boolean
   // would act repeatedly; callers must consult
   // millisec_since_resume_detected_by_clocktime() and act only on the
   // remainder of their own interval, as does
   // dw_pause_if_recently_resumed_from_sleep() does.

   // Sample on every call, including inside the grace window.  Returning
   // early from the window without sampling would leave the baseline stale,
   // so a second suspend beginning during the window went undetected: the
   // window would expire measured from the FIRST resume while the second had
   // never been seen at all.
   uint64_t current_accumulated_sleep_ns  = get_accumulated_sleep_ns();

   // Work on a copy of the per-thread baseline, written back only when
   // mutation is allowed.  Seeding it is itself a mutation, so an observer
   // must not do that either.
   uint64_t previous_ns = previous_accumulated_sleep_ns;
   if (previous_ns == UINT64_MAX) {
      // First call on this thread: seed from the global baseline if available,
      // otherwise fall back to current value (no resume detectable this call).
      previous_ns = (global_initial_accumulated_sleep_ns != UINT64_MAX)
               ? global_initial_accumulated_sleep_ns
               : current_accumulated_sleep_ns;
   }
   // Accumulated sleep is the difference of two separately sampled clocks,
   // so successive values jitter by the interval between the two reads,
   // a few microseconds, and are not monotonic.  Subtracting unsigned when
   // the newer value is the smaller wraps to nearly 2**64, which passes the
   // threshold test below and reports a resume that never occurred.  Guard
   // the subtraction, and on a decrease take the lower value as the new
   // baseline so that a high sample is not latched, leaving every
   // subsequent sample looking like a decrease.

   // Compare in uint64_t nanoseconds throughout -- narrowing to int
   // milliseconds before comparing would overflow for a suspend
   // longer than ~24.8 days (INT_MAX ms).
   uint64_t sleep_increase_ns = 0;
   if (current_accumulated_sleep_ns > previous_ns)
      sleep_increase_ns = current_accumulated_sleep_ns - previous_ns;
   else
      previous_ns = current_accumulated_sleep_ns;
   const uint64_t detection_threshold_secs = 1;
   const uint64_t detection_threshold_ns =  SECS2NANOS(detection_threshold_secs);

   // n.b. the increase is sleep accumulated since this thread's baseline was
   // last written, not the duration of the most recent suspend.  A suspend
   // shorter than the threshold does not update the baseline, so its sleep
   // remains in the sum.  Do not read this value as one suspend's length.
   if (sleep_increase_ns > detection_threshold_ns) {
      // Accumulated sleep grew by > detection_threshold_secs since previous => we resumed.
      resumed = true;
      detected_now = true;
      uint64_t prior_ns = previous_ns;
      previous_ns = current_accumulated_sleep_ns;
      if (!no_mutate) {
         // Not logged by an observer: it has not consumed the detection, so
         // the next real call will detect and log it again, and one resume
         // would appear in the log twice.
         SIMPLE_STD_FUNC_SYSLOG(LOG_INFO,
               "Resume from sleep detected by BOOTTIME/MONOTONIC, sleep increase=%"PRIu64" ms, "
               "previous=%"PRIu64" ms, current=%"PRIu64" ms",
               NANOS2MILLIS(sleep_increase_ns),
               NANOS2MILLIS(prior_ns),
               NANOS2MILLIS(current_accumulated_sleep_ns));
         most_recent_detection_ms = cur_boottime_ms;
      }
   }
   else if (most_recent_detection_ms != UINT64_MAX &&
            (cur_boottime_ms - most_recent_detection_ms) < 5000)
   {
      resumed = true;
      if (!no_mutate)
         SIMPLE_STD_FUNC_SYSLOG(LOG_DEBUG, "Called within 5 sec of reset");
   }

   if (!no_mutate)
      previous_accumulated_sleep_ns = previous_ns;

   DBGF(debug, "no_mutate=%s, previous_ns=%"PRIu64", current_accumulated_sleep_ns=%"PRIu64
               ", detected_now=%s, returning %s",
               sbool(no_mutate),
               NANOS2MILLIS(previous_ns),
               NANOS2MILLIS(current_accumulated_sleep_ns),
               sbool(detected_now), sbool(resumed));

   if (detected_now_loc)
      *detected_now_loc = detected_now;
   return resumed;
}


/** Detects whether the system has resumed from sleep, updating the detector's
 *  per-thread state.  Equivalent to recently_resumed_from_sleep_by_clocktime0()
 *  with no_mutate false.
 *
 *  @param  detected_now_loc  see recently_resumed_from_sleep_by_clocktime0()
 *  @return true if a resume from sleep was detected on this call, or was
 *          detected on this thread within the past 5 seconds
 */
bool recently_resumed_from_sleep_by_clocktime(bool * detected_now_loc) {
   return recently_resumed_from_sleep_by_clocktime0(false, detected_now_loc);
}


/** Returns the number of milliseconds since a resume from sleep was last
 *  detected on this thread by recently_resumed_from_sleep_by_clocktime().
 *
 *  Lets callers that pause for a fixed interval after a resume sleep only
 *  the time remaining in that interval, rather than the full interval on
 *  every call within the detector's grace window.
 *
 *  @return milliseconds since the resume was detected,
 *          UINT64_MAX if no resume has been detected on this thread
 */
uint64_t millisec_since_resume_detected_by_clocktime() {
   if (most_recent_detection_ms == UINT64_MAX)
      return UINT64_MAX;
   return NANOS2MILLIS(cur_boot_time_nanosec()) - most_recent_detection_ms;
}
