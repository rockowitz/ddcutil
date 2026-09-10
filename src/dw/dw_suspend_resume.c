/** @file dw_suspend_resume.c
 *  Detection of system resume from sleep, combining every available method
 *
 *  Consults the logind PrepareForSleep signals recorded by dbus_util.c together
 *  with the clock comparison in util/suspend_resume_util.c.  The clock half is
 *  separate because it needs no dbus and has a util-layer consumer; this half
 *  does need dbus and belongs with the display watch code that drives it.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <syslog.h>
#include "util/debug_util.h"
#include "util/dbus_util.h"
#include "util/report_util.h"
#include "util/string_util.h"
#include "util/syslog_util.h"
#include "util/timestamp.h"
#include "util/suspend_resume_util.h"

#include "dw_suspend_resume.h"

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
   //
   // This branch is the one that reports a resume when the clock method is
   // wrong, and it is reached whatever dbus says, so a false detection here
   // is not masked by a correct dbus answer.  It surfaced as
   // test_dw_suspend_resume failing on the aarch64 worker at
   // build.opensuse.org while passing on x86_64: the two assertions in that
   // test expecting false are the only ones this branch can reach.  The
   // cause was in get_accumulated_sleep_ns(), not here or in the test --
   // nothing about it is specific to aarch64, only to a machine whose
   // BOOTTIME/MONOTONIC offset sits near zero, which is every worker that
   // has never suspended.
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
