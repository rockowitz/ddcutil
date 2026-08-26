/** @file suspend_resume_util.c
 *
 *  Detection of system suspend and resume.
 *
 *  Answers one question, in several ways: has this process recently resumed
 *  from a system sleep state, and how long ago?  It matters because the
 *  /dev/i2c device ACLs are dropped and reapplied around a suspend cycle, so
 *  an open() attempted in the window just after resume can fail with EACCES
 *  though nothing is permanently wrong.
 *
 *  Three detection methods are used.  They are complementary, not redundant,
 *  and none alone is sufficient; see the comment block preceding
 *  #recently_resumed_from_sleep() for the full account.
 *
 *  This file is about the sleep states the **system** enters.  The delays
 *  ddcutil itself takes are elsewhere: base/sleep.h and base/tuned_sleep.h.
 */

// Copyright (C) 2021-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifndef TARGET_BSD
#include <syslog.h>
#endif
/** \endcond */

#include "debug_util.h"
#ifdef USE_DBUS
#include "dbus_util.h"
#endif
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
 *  @param accumulated sleep time in nanaosec
 */
static uint64_t get_accumulated_sleep_ns() {
   struct timespec bt;
   struct timespec mt;
   clock_gettime(CLOCK_BOOTTIME,  &bt);  // advances during sleep
   clock_gettime(CLOCK_MONOTONIC, &mt);  // does not advance during sleep
   uint64_t boottime_ns = SECS2NANOS(bt.tv_sec) + bt.tv_nsec;
   uint64_t mono_ns     = SECS2NANOS(mt.tv_sec) + mt.tv_nsec;
   uint64_t accumulated_sleep_ns = boottime_ns - mono_ns;
   return accumulated_sleep_ns;
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
 *          no resume.  A caller weighing this detector against another
 *          source needs the distinction: a detection on this call means
 *          sleep accumulated that this thread had not yet accounted for, so
 *          another source may not have processed the corresponding event
 *          either.  Within the grace window that is no longer true, the
 *          resume having been observed at least once already.
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
   // With the window, "recently
   // resumed" is a state that any number of callers can query for a
   // bounded period.  The cost is that true is returned for the whole
   // window, so a caller acting on the bare boolean would act repeatedly;
   // callers must consult millisec_since_resume_detected_by_clocktime()
   // and act only on the remainder of their own interval, as
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


//
// Combined CLOCKTIME/BOOTTIME and dbus algorithm
//

/** Determines whether the system recently resumed from sleep, consulting
 *  every available detection method.
 *
 *  @param  within_ms           interval that defines "recently"
 *  @param  millisec_since_loc  if non-NULL, where to return the number of
 *                              milliseconds since the resume, UINT64_MAX if
 *                              no recent resume
 *  @param  detection_loc       if non-NULL, where to return which method
 *                              answered.  A caller that reports the pause it
 *                              takes needs this: only two of the three
 *                              methods establish that the system has actually
 *                              slept.  See resume_detection_description().
 *  @return true if a resume from sleep occurred within the past **within_ms**
 *
 *  @remark
 *  Callers pause for the time remaining in their own interval, rather than
 *  this function pausing, because each has its own sleep and logging needs.
 *
 *  ddcutil detects a resume from sleep three ways.  They are complementary,
 *  not redundant, and none alone is sufficient.
 *
 *  The **dbus** method (dbus_util.c) records when the logind
 *  **PrepareForSleep(false)** signal is received.  It is precise, it is
 *  process wide, and it covers three cases the clock method cannot:
 *   - **Program start.**  ldbus_elapsed_since_resume_from_sleep_mark_start()
 *     deliberately treats the start of the sleep watch thread like a resume,
 *     because the window just after boot or login has the same transient
 *     EACCES race on /dev/i2c opens that the window after resume does.  The
 *     clock method reports nothing at startup: no sleep has accumulated.
 *   - **Short suspends.**  The clock method needs more than 1 second of
 *     accumulated sleep before it reports a resume at all.
 *   - **Shared timing.**  last_resume_from_sleep_ns is a single process wide
 *     value, so every thread computes the same elapsed time and pauses only
 *     the remainder of the interval.  The clock method's state is per thread,
 *     so threads detect independently and each starts its own pause from its
 *     own first look, which can multiply the pauses taken.
 *  Its weakness is latency.  The signal is delivered asynchronously, and the
 *  display watch thread can be woken by udev and begin reopening buses before
 *  the dbus thread has processed it.  That interval is exactly when udev has
 *  not yet reapplied the /dev/i2c ACLs, so relying on dbus alone mistakes a
 *  transient permission failure for a permanent one.
 *
 *  The **clock** method (recently_resumed_from_sleep_by_clocktime()) compares
 *  CLOCK_BOOTTIME against CLOCK_MONOTONIC.  It cannot be late: the divergence
 *  is already present the instant the thread runs again, and no signal
 *  delivery is involved.  Its weaknesses mirror the strengths above.  It is
 *  coarse, per thread, silent at startup, and its reference point is when a
 *  thread happened to look, not when the resume occurred.
 *
 *  dbus is therefore preferred where it is trustworthy, and the clock method
 *  covers the case it cannot: the signal not yet delivered.
 *
 *  The **open sleep cycle** method uses the other logind signal,
 *  **PrepareForSleep(true)**, whose timestamp dbus_util.c records alongside
 *  the resume timestamp.  A cycle opened by that signal and not yet closed by
 *  its counterpart means this process is somewhere inside a suspend, and any
 *  thread running there is treated as having just resumed.  It covers what
 *  neither of the others can: a suspend that stopped this process without
 *  accumulating sleep the clock method can see.  Its weakness is that it
 *  depends on the closing signal to know the cycle is over; the sleep watch
 *  thread's own running time bounds what happens when that signal never
 *  arrives.  See the body and ldbus_in_open_sleep_cycle().
 *
 *  Neither elapsed time can simply be trusted over the other:
 *   - Taking whichever is smaller prefers the clock method systematically,
 *     since its reference point is the later one whenever detection lags the
 *     resume, and pauses for the full interval well after the resume: resume
 *     at T, dbus signal at T+50 ms, first call on this thread at T+3000 ms,
 *     elapsed reported as 0 rather than 3000.
 *   - Taking dbus whenever it is within the interval lets a timestamp that
 *     predates the suspend, from an earlier resume or from
 *     ldbus_elapsed_since_resume_from_sleep_mark_start(), mask a detection
 *     that is genuinely fresh, and pauses too little.
 *
 *  The tie is broken on whether the clock method detected the resume on this
 *  very call, which it reports through its out parameter.  A detection now
 *  means this thread had not yet accounted for the sleep, so dbus may not
 *  have processed the signal either and its timestamp may predate the
 *  suspend; the clock is preferred.  Inside the grace window the resume has
 *  already been observed once, dbus has had time to catch up, and it is the
 *  more accurate of the two.  See the body for why the two errors are not
 *  symmetric.  Note that the decision deliberately does not depend on
 *  within_ms, which is tunable and must not determine correctness.
 */
bool recently_resumed_from_sleep(int within_ms, uint64_t * millisec_since_loc,
                                 Resume_Detection * detection_loc)
{
   bool debug = false;
   bool resumed = false;
   uint64_t millisec_since = UINT64_MAX;
   Resume_Detection detection = RESUME_DETECTED_NONE;

   // Called on every invocation, whatever dbus reports, so that this thread's
   // baseline stays current and its grace window opens when the resume is
   // first observed here.  Otherwise the first call on a thread where dbus
   // always won the race would report a resume that was long since handled.
   bool clock_detected_now = false;
   bool resumed_by_clocktime = recently_resumed_from_sleep_by_clocktime(&clock_detected_now);

#ifdef USE_DBUS
   // An open sleep cycle, i.e. a PrepareForSleep(true) not yet matched by a
   // PrepareForSleep(false), is reported as a resume whatever the clocks say.
   //
   // Once the kernel has frozen user space this process cannot run again until
   // it is thawed, so a thread executing inside an open cycle has either been
   // thawed already or is in the interval between the signal and the freeze.
   // Neither other source covers the first case:
   //  - dbus has not yet dispatched PrepareForSleep(false).  That is the
   //    latency described above, and the resume timestamp it would report
   //    still predates the suspend.
   //  - the clock method needs more than a second of accumulated sleep, which
   //    the cycle need never have produced.  Freezing user space precedes
   //    timekeeping_suspend() and thawing follows timekeeping_resume(), so a
   //    suspend that is aborted, or whose device callbacks are slow (hybrid
   //    graphics, notably), can stop this process for ten seconds while
   //    BOOTTIME and MONOTONIC stay in lockstep and nothing is detected.  The
   //    /dev/i2c ACLs are dropped and reapplied around the whole cycle, not
   //    around its sleeping part, so the EACCES window is there regardless.
   //
   // Reported as elapsed 0, so the caller pauses its full interval.  While the
   // cycle is open there is no better reference point: the prepare timestamp
   // marks the start of the cycle, not the resume, and measuring from it would
   // count the entire suspend as already elapsed and pause not at all.
   //
   // Before the freeze this reports a resume that has not occurred.  That
   // costs one interval per call over a window logind bounds by
   // InhibitDelayMaxSec, 5 seconds by default, and ddcutil holds no delay
   // inhibitor, so the freeze normally follows the signal promptly.  It also
   // keeps the watch thread from opening buses while the GPU is being torn
   // down, which is no worse a place to be idle.
   //
   // The rule holds until the matching signal arrives, or, should it never
   // arrive, until the sleep watch thread has run long enough since the
   // prepare signal to conclude it is not coming.  See
   // ldbus_in_open_sleep_cycle().
   bool sleep_cycle_open = ldbus_in_open_sleep_cycle(NULL);

   // within_ms 0 asks whether a resume occurred within no time at all, and the
   // answer must remain no: callers subtract millisec_since from within_ms.
   if (sleep_cycle_open && within_ms > 0) {
      resumed = true;
      millisec_since = 0;
      detection = RESUME_DETECTED_IN_SLEEP_CYCLE;
   }

   uint64_t dbus_elapsed_ms = NANOS2MILLIS(ldbus_elapsed_since_resume_from_sleep_ns());

   // A detection on THIS call means sleep accumulated that this thread had
   // not yet accounted for, so dbus may not have processed the corresponding
   // signal either; its timestamp can predate the suspend, and measuring
   // from it would pause far too little.  The clock is preferred in that
   // case.  Within the grace window the resume has been observed at least
   // once already, dbus has had time to catch up, and its timestamp is the
   // more accurate of the two, so it is preferred there.
   //
   // The two errors are not symmetric, which is what settles the direction.
   // Preferring the clock when dbus was in fact current costs one interval
   // of extra pause, latency and nothing more.  Preferring dbus when its
   // timestamp is stale reopens the buses while udev has not yet reapplied
   // the ACLs, which costs the whole EACCES retry ladder, up to
   // max_eacces_retry_ms, plus a diagnostic dump in the system log.
   //
   // Deliberately not decided by comparing the two elapsed times, nor by
   // comparing either against within_ms: within_ms is tunable and must not
   // be what determines correctness.
   //
   // Not consulted when the cycle is open: its timestamp is then known to
   // predate the suspend, and 0 is already the conservative answer.
   if (!resumed && !clock_detected_now && dbus_elapsed_ms < (uint64_t) within_ms) {
      resumed = true;
      millisec_since = dbus_elapsed_ms;
      detection = RESUME_DETECTED_BY_DBUS;
   }
#endif

   // Fallback, for the case dbus cannot cover: the signal has not yet been
   // delivered, or the build has no dbus support.
   if (!resumed && resumed_by_clocktime) {
      uint64_t clock_elapsed_ms = millisec_since_resume_detected_by_clocktime();
      if (clock_elapsed_ms < (uint64_t) within_ms) {
         resumed = true;
         millisec_since = clock_elapsed_ms;
         detection = RESUME_DETECTED_BY_CLOCKTIME;
      }
   }

   if (millisec_since_loc)
      *millisec_since_loc = millisec_since;
   if (detection_loc)
      *detection_loc = detection;
   DBGF(debug, "within_ms=%d, millisec_since=%"PRIu64", detection=%s, returning %s",
               within_ms, millisec_since, resume_detection_description(detection),
               sbool(resumed));
   return resumed;
}


/** Returns a description of how a resume from sleep was detected, phrased as
 *  the opening clause of a message reporting it.
 *
 *  An open sleep cycle is deliberately not described as a resume.  The pause
 *  it causes may be taken before the system has slept at all, in the interval
 *  between PrepareForSleep(true) and the freeze, and a message claiming a
 *  resume there would contradict the machine's state in the system log at
 *  exactly the point where this subsystem is diagnosed from it.
 *
 *  @param  detection  value reported by #recently_resumed_from_sleep()
 *  @return description, valid for the life of the program
 */
const char * resume_detection_description(Resume_Detection detection) {
   char * result = "Unrecognized resume detection";
   switch(detection) {
   case RESUME_DETECTED_NONE:
      result = "No recent resume from sleep";                       break;
   case RESUME_DETECTED_BY_DBUS:
      result = "Recently resumed from sleep, per dbus signal";      break;
   case RESUME_DETECTED_BY_CLOCKTIME:
      result = "Recently resumed from sleep, per clock comparison"; break;
   case RESUME_DETECTED_IN_SLEEP_CYCLE:
      result = "Sleep cycle in progress";                           break;
   }
   return result;
}
