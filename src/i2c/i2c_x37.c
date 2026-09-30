/** @file i2c_x37.c
 *  Detection of slave address x37, which ddcutil takes as evidence that a display
 *  supports DDC/CI, and the exploration functions used to characterize how a bus
 *  responds to the several ways an I2C address can be probed.
 *
 *  What each probe establishes, and what it does not, is set out at the functions
 *  themselves.
 */

// Copyright (C) 2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

/** \cond */
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ioctl.h>
/** \endcond */

#ifdef TARGET_BSD
#include "bsd/i2c-dev.h"
#else
#include "i2c/wrap_i2c-dev.h"
#endif

#include "util/coredefs_base.h"
#include "util/file_util.h"
#include "util/string_util.h"

#include "base/core.h"
#include "base/parms.h"
#include "base/rtti.h"
#include "base/sleep.h"

#include "i2c/i2c_strategy_dispatcher.h"

#include "i2c/i2c_x37.h"

// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_I2C;

// Set from --skip-ddc-checks, which lives in the ddc layer and so cannot be read from
// here.  See check_x37_for_businfo() for why the option implies assuming x37 responsive
// rather than merely not probing.
bool skip_x37_detection = false;


/*
 * The test is successful only if ioctl(fd, I2C_SMBUS, &smbus) == 0.
That indicates the controller completed the Quick Write transaction,
including an ACK from the addressed target.
A failure such as ENXIO, EREMOTEIO, or sometimes EIO generally indicates a NACK or
unsuccessful transaction, but the exact errno depends on the host-controller driver.

What goes on the bus:
An SMBus Quick Write typically looks like:

   START  [0x37 << 1 | 0]  ACK  STOP
           0x6e

There is no command byte and no data byte.
The R/W bit is zero because it is a “write” quick command.
The 0x6e shown on the wire is the 8-bit address byte;;
The C program uses the normal 7-bit address:

Why not write(fd, NULL, 0)?
It is a zero-byte file write, not necessarily a zero-data-byte I2C transfer.
It may return 0 without making any hardware transaction.
Likewise, this may be accepted as a local no-op:

   uint8_t dummy;
   write(fd, &dummy, 0);

Do not use either return value to claim that 0x37 is present.

Technically, the Linux raw-I2C path uses struct i2c_msg, whose len can be zero,
but whether an adapter can—or does—generate a usable zero-length transfer is controller-
and driver-dependent. It is not a portable address-probe interface. I2C_SMBUS_QUICK exists
to represent the intended address-only SMBus transaction and must be gated on I2C_FUNC_SMBUS_QUICK.
(see https://docs.kernel.org/driver-api/i2c.html)

Caveats:

- Quick Write can have side effects. Although it carries no data byte,
  some devices interpret the address-only transaction as a reset,
  trigger, FIFO action, state change, or an invalid protocol condition.

- A successful ACK proves that something at 0x37 responded;
  it does not prove it is the expected part or operating correctly.

- A device may ACK a conventional register read but reject SMBus Quick commands.
  In that case, “Quick Write failed” does not conclusively mean the device is absent.

- If the address is already controlled by a kernel driver, do not force access
   with I2C_SLAVE_FORCE; use the driver’s exposed kernel interface for
   health/status instead. (general comment, no known method for amdgpu etc.)
 */


int probe_i2c_quick(int fd, Byte addr,  bool write) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "addr=0x%02x, write=%s", addr, sbool(write));

   int result = 0;
   unsigned long funcs;

   struct i2c_smbus_ioctl_data smbus = {
       .read_write = I2C_SMBUS_READ,
       .command = 0,
       .size = I2C_SMBUS_QUICK,
       .data = NULL,
   };

   if (write)
         smbus.read_write = I2C_SMBUS_WRITE;

   int rc = 0;

   errno=0;
   rc = ioctl(fd, I2C_FUNCS, &funcs);
   if (rc < 0) {
           //result = -errno;
           result = rc;
           goto bye;
   }
   errno = 0;
   rc = ioctl(fd, I2C_SLAVE, addr);
   if (rc < 0) {
           result = -errno;
           DBGTRC(debug, DDCA_TRC_NONE, "ioctl I2C_SLAVE failed, addr=0x%02x rc = %d, errno=%s",
                 addr,rc, psc_desc(-result));
           goto bye;
   }

   rc = ioctl(fd, I2C_SMBUS, &smbus);
   if (rc < 0) {
           result = -errno;
           DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "%s failed, rc=%d, errno=%s",
                 (write) ? "SMBUS_QUICK_WRITE" : "SMBUS_QUICK_READ", rc, psc_desc(-result));
   }

bye:
   DBGTRC_RET_DDCRC(debug, TRACE_GROUP,result, "");
   return result;
}


#ifdef EXPLORATORY
void explore_smbus_quick(int fd) {
   int debug = true;
   char* fn = NULL;
   filename_for_fd(fd, &fn);
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "%s", fn);

   Byte addrs[3] = {0x37, 0x40, 0x50};

   Status_Errno rc = 0;




   for (int ndx= 0; ndx< 3; ndx++) {
       rc = probe_i2c_quick(fd, addrs[ndx],  /*write*/ true);
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
             "probe_i2c_quick(), %s, addr=0x%02x', %s returned %d, errno=%d",
             fn,
         addrs[ndx], "write", rc, errno);

       rc = probe_i2c_quick(fd, addrs[ndx],  /*write*/ false);
       DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
             "probe_i2c_quick(), %s.  addr=0x%02x', %s returned %d, errno=%d",
             fn,
         addrs[ndx], "read", rc, errno);
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}
#endif


/** Issues one I2C_RDWR transfer, for the benefit of explore_smbus_probes().
 *
 *  @param  fd       file descriptor for open /dev/i2c-n
 *  @param  addr     7 bit slave address
 *  @param  len      transfer length, may be 0
 *  @param  is_read  true for a read, false for a write
 *  @param  buf      data buffer, unused when len is 0
 *  @return 0 if the transfer succeeded, otherwise the negated errno
 */
STATIC Status_Errno
probe_i2c_rdwr(int fd, Byte addr, int len, bool is_read, Byte * buf) {
   struct i2c_msg              messages[1];
   struct i2c_rdwr_ioctl_data  msgset;
   memset(messages, 0, sizeof(messages));   // valgrind, see i2c_ioctl_writer()
   memset(&msgset,  0, sizeof(msgset));

   messages[0].addr  = addr;
   messages[0].flags = (is_read) ? I2C_M_RD : 0;
   messages[0].len   = len;
   messages[0].buf   = buf;
   msgset.msgs  = messages;
   msgset.nmsgs = 1;

   errno = 0;
   int rc = ioctl(fd, I2C_RDWR, &msgset);
   return (rc < 0) ? -errno : 0;
}


/** Reports how each way of probing a slave address behaves on this bus.
 *
 *  Six transfers per address: SMBus Quick write and read, a zero length write and
 *  read, and a one byte write and read.  The one byte read is the only one of the six
 *  that is a full transaction on every adapter, so it is the best available reference
 *  and the report flags any probe that disagrees with it.
 *
 *  It is a reference, not an oracle.  A one byte read can fail at an address where a
 *  device is present but does not support being read: 0x30, the E-DDC segment pointer,
 *  is write only, and on i915 all four address-only probes are ACKed there while both
 *  one byte transfers return ENXIO.  Read a disagreement as a question about that
 *  address, and only treat it as misreporting where something is independently known to
 *  answer, as at 0x50 and at 0x37 on a display whose DDC/CI works.
 *
 *  @param  fd      file descriptor for open /dev/i2c-n
 *  @param  driver  driver name, reported so results can be attributed
 *
 *  @remark
 *  Why this exists, and what it established.  i2c_detect_x37() tries a one byte read
 *  at 0x37 and falls back to a zero length write.  Measured over five runs each on four
 *  adapters -- i915, amdgpu and the two buses of one nvidia card -- no probe of the six
 *  is correct everywhere.  But the two errors are not equally costly, and that decides
 *  which probe to prefer.  A false positive is caught at the DDC layer, which issues a
 *  real request and validates the reply.  A false negative stops the DDC layer being
 *  consulted at all, so a working display loses DDC/CI silently.  Scoring only false
 *  negatives, meaning a probe failing at 0x50 where the EDID EEPROM demonstrably
 *  answers, the zero length write is the only one of the six with none on any bus.  The
 *  one byte read, the zero length read and the one byte write each fail on amdgpu, the
 *  one byte read intermittently; SMBus Quick fails on the nvidia DisplayPort bus.
 *
 *  @remark
 *  So the order in i2c_detect_x37() is right, and for a reason opposite to the one
 *  an earlier version of this comment gave.  The fallback can only turn a negative into
 *  a positive, never the reverse, so it strictly reduces false negatives; that its
 *  successes are sometimes meaningless is the harmless direction.  Across all seven
 *  buses with a display the two step probe produced no false negative at all, and at
 *  0x37 the one byte read succeeded on every one of them, so the fallback never fired --
 *  it is insurance, and the one adapter where the read is intermittent is exactly where
 *  the zero length write holds.  Switching to SMBus Quick would be a regression: on the
 *  nvidia DisplayPort bus Quick fails at 0x37 and at 0x50, so a Quick based probe would
 *  report x37 unresponsive for a working MCCS 2.1 display and the DDC layer would never
 *  see it.  I2C_FUNC_SMBUS_QUICK is advertised by every adapter tested, that bus
 *  included, so it cannot be used to gate against this.
 *
 *  @remark
 *  The anomalies cluster by connector type rather than by driver.  All three native DDC
 *  buses, i915 HDMI and the amdgpu and nvidia DVI-D buses, reject 0x40 on every probe.
 *  Every failure to discriminate is on a DisplayPort bus, where I2C is tunnelled over
 *  AUX: both i915 DP buses ACK every address and return 0x6e at 0x40 as readily as at
 *  0x37, so on those no address probe of any shape can mean anything; the nvidia DP bus
 *  has Quick always failing and zero length always succeeding.  The amdgpu DP bus is the
 *  best behaved of the four and the only bus anywhere returning EREMOTEIO, the proper
 *  errno for a NACK.
 *
 *  @remark
 *  The one byte write is included for completeness and is the one transfer here that
 *  puts a byte on the wire.  At 0x50 that is the EDID word offset write ddcutil
 *  performs anyway.  At an arbitrary address it is a command byte to whatever may be
 *  listening, so this function is for exploration on known hardware, not for
 *  production paths.
 */
void explore_smbus_probes(int fd, char * driver) {
   bool debug = false;
   char * fn = NULL;
   filename_for_fd(fd, &fn);
   DBGTRC_STARTING(debug, DDCA_TRC_NONE, "%s, driver=%s", fn, driver);

   Byte addrs[] = {0x30, 0x37, 0x40, 0x50};
   Byte databyte = 0x00;

   for (uint ndx = 0; ndx < ARRAY_SIZE(addrs); ndx++) {
      Byte addr = addrs[ndx];

      // One psc_desc() per DBGTRC call.  psc_desc() formats into a single per thread
      // buffer and returns it, so two calls in one argument list both yield the same
      // pointer and print the same text; reporting a differing pair on one line
      // silently shows one status twice.
      struct { const char * name; Status_Errno rc; } probes[6];
      probes[0].name = "SMBus quick write";
      probes[0].rc   = probe_i2c_quick(fd, addr, /*write*/ true);
      probes[1].name = "SMBus quick read";
      probes[1].rc   = probe_i2c_quick(fd, addr, /*write*/ false);
      probes[2].name = "zero length write";
      probes[2].rc   = probe_i2c_rdwr(fd, addr, 0, /*is_read*/ false, &databyte);
      probes[3].name = "zero length read";
      probes[3].rc   = probe_i2c_rdwr(fd, addr, 0, /*is_read*/ true,  &databyte);
      probes[4].name = "one byte write";
      probes[4].rc   = probe_i2c_rdwr(fd, addr, 1, /*is_read*/ false, &databyte);
      Byte readbyte  = 0x00;
      probes[5].name = "one byte read";
      probes[5].rc   = probe_i2c_rdwr(fd, addr, 1, /*is_read*/ true,  &readbyte);
      Status_Errno one_read = probes[5].rc;

      DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "%s, addr=0x%02x, driver=%s:", fn, addr, driver);
      for (uint k = 0; k < ARRAY_SIZE(probes); k++)
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "   %-18s %s", probes[k].name, psc_desc(probes[k].rc));
      if (one_read == 0)
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "   one byte read returned 0x%02x", readbyte);

      // name any probe that disagrees with the one byte read
      for (uint k = 0; k < ARRAY_SIZE(probes)-1; k++) {
         if (one_read == 0 && probes[k].rc != 0)
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                  "   DISAGREES: %s failed where the one byte read succeeded", probes[k].name);
         else if (one_read != 0 && probes[k].rc == 0)
            DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE,
                  "   DISAGREES: %s succeeded where the one byte read failed", probes[k].name);
      }
   }

   DBGTRC_DONE(debug, DDCA_TRC_NONE, "");
}


/** Tests whether slave address x37 is responsive, which ddcutil takes as evidence
 *  that the display supports DDC/CI.
 *
 *  @param  fd      file descriptor for open /dev/i2c-n
 *  @param  driver  driver name
 *  @return status code, 0 if the address responded
 *
 *  @remark
 *  The remaining remarks are Claude generated.  They contain a lot of nonsense,
 *  reflecting changed assumptions and Claude's evolving understand.  They need
 *  to be edited.
 *
 *
 *  @remark
 *  What the VESA standards say about probing this address: nothing.  E-DDC's scope
 *  is getting the EDID or DisplayID out of the display -- the EEPROM at 7 bit
 *  0x50, the segment pointer at 0x30 for access past 256 bytes, and the
 *  requirement that the EDID be readable whenever the display has power.  It
 *  mentions the 0x6E/0x6F pair only as the address pair belonging to the DDC2Bi
 *  and DDC2B+ command channel.  It defines no procedure for determining whether
 *  that channel exists, and no concept of address responsiveness at all.
 *
 *  @remark
 *  The document that owns 0x37 is the DDC/CI standard, and what it defines is a
 *  message protocol: destination address, source address, a length byte with the
 *  0x80 high bit set, payload, and an XOR checksum in which the host's virtual
 *  address participates.  Its answer to "is DDC/CI present" is at that level, and
 *  it is host initiated throughout: the display is a slave and every message it
 *  sends is a response.  The Null Message, 6E 80 BE, is one such response, returned
 *  to a valid request the display has no data for or does not support.  It is not
 *  something a display emits because it has nothing to say, so an unsolicited read
 *  of 0x6F has no defined result at all -- what comes back is whatever the output
 *  buffer holds.  The sanctioned test is therefore to issue an actual request --
 *  Identification Request, Capabilities Request, or a VCP feature read -- and
 *  validate the reply's length and checksum.  A bare address probe, of any length,
 *  sits below the level either standard describes.
 *
 *  @remark
 *  So this test is a heuristic, wrong in both directions, and neither direction is
 *  a violation by the monitor.  An ACK does not imply DDC/CI works: nothing
 *  obliges a device that ACKs 0x6E to produce a well formed reply.  No ACK does
 *  not imply no DDC/CI: a zero length write is a degenerate transaction that some
 *  adapters will not emit and some drivers synthesize a result for, and a display
 *  in standby may not answer on 0x6E though E-DDC still requires it to answer on
 *  0xA0.  That asymmetry is why an x37 result is less durable than an EDID result,
 *  and it is the justification for the x37 detection table.
 *
 *  @remark
 *  This function is called only where an EDID was obtained, the check sitting
 *  inside "else if (businfo->edid)" in i2c_check_bus(), so it never probes a silent
 *  bus.  Measured cost where it does run is negligible: 0.25 ms and 0.69 ms on
 *  i915, 0.27 ms on amdgpu, the payload being one byte and a device being known to
 *  answer.  The amdgpu asymmetry that governs the EDID readers, roughly 11 ms for a
 *  write only transfer against 650 ms for a read whose payload reaches the engine,
 *  therefore does not bear on this probe at all.
 *
 *  @remark
 *  Both probes are ACK tests and the byte read is discarded.  An earlier version of
 *  this comment claimed the 1 byte read was at least the opening byte of a null
 *  message, so that a display following the spec should return 0x6E.  Measurement
 *  says otherwise.  Across three drivers and six displays the first byte came back
 *  0x53, 0x30 and 0x00 on i915, 0xbe on amdgpu, and 0x6e and 0x02 on nvidia.  The
 *  amdgpu trace shows why the value is arbitrary -- the display emits 6e 80 be
 *  repeatedly and a read lands at an arbitrary point in that stream, 0xbe being the
 *  third byte of a null message.  The nvidia pair settles what the value is worth:
 *  0x6e, the one byte that would pass a naive test, came from an HP LP1965 whose
 *  DDC/CI does not work at all, its VCP version detection failing with
 *  DDCRC_RETRIES, while the Dell U2719DX on the same adapter, whose DDC/CI works,
 *  returned 0x02.  The byte is not merely uninformative, it points the wrong way.  So the read is defensible on other
 *  grounds, that it is cheap, that it is non-destructive where a write probe on
 *  0x30 to 0x37 need not be, and that on a responsive display it is a single ACKed
 *  transaction, but not on the content of what it returns.  The zero length write
 *  corresponds to nothing in either document and, every monitor tested having ACKed
 *  the read, has never been exercised.
 *
 *  @remark
 *  Reading one byte and discarding it does leave the display partway through a
 *  message, but no harm from that has been observed: the first full DDC/CI read
 *  after the probe was correctly aligned on both drivers.  Misalignment does appear
 *  later in a session, reads beginning "be 80 be" or "80 80 be", in a stream the
 *  display emits continuously, so the probe is not shown to be the cause.
 *  Comparing the phase of the first DDC/CI read with the probe and without would
 *  settle it.
 *
 *  @remark
 *  Reading the full Null Message and checking it was tried and removed.  The
 *  attraction was that an ACK test cannot distinguish something answering at 0x37
 *  from a DDC/CI display answering there, which is exactly the failure the comments
 *  in i2c_check_bus() record -- a laptop display reporting x37 active without
 *  responding to DDC, and a U3011 with DDC turned off still showing x37 detected.
 *  Six bytes were read and scanned for 6e 80 be at any phase.  On amdgpu the Null
 *  Message was found every time.  On i915 it was found on none of three buses
 *  with working monitors, which returned "53 e7 ae 62 54 be", "30 31 31 29 63 6d"
 *  and "00 00 00 00 03 00"; the second of those is ASCII "011)cm", stale bytes of a
 *  capabilities string from an earlier conversation, which is what an empty output
 *  buffer hands back.  The i915 result is not misbehavior: no request precedes this
 *  read, and the Null Message is a response to a request, so a display owes nothing
 *  here.  Finding it is positive evidence; not finding it proves nothing.  That is
 *  the reason it cannot be the verdict, the measurement merely showing what the
 *  protocol already implies, and making it decisive would have reported three
 *  working monitors as unresponsive at x37.  With the result unable to inform
 *  anything, a six byte read only consumed five more bytes of a buffer whose
 *  contents matter to whatever reads next, so the one byte read is restored.
 *
 *  @remark
 *  A probe that would be sound by the letter of DDC/CI has to send a request and
 *  validate the response, which is what the DDC layer already does.  That is the
 *  same conclusion the comments in i2c_check_bus() reached when DDC checking was
 *  moved there entirely, and it is why no amount of refining this read turns it
 *  into a support test.  What is left for it to do is what it does now: establish
 *  cheaply that something is still at 0x37, so that cached display information is
 *  not reloaded for a display that has gone away.
 *
 *  @remark
 *  Worth keeping from that experiment: a read at 0x37 does not begin on a message
 *  boundary.  Of three consecutive amdgpu probes two began mid message, returning
 *  "be 80 be 6e 80 be" where the third returned "6e 80 be 6e 80 be".  Any future
 *  attempt to interpret bytes read here has to tolerate phase rather than assume
 *  alignment, or it will look like an intermittent fault.
 *
 *  @remark
 *  The account above is from knowledge of the two standards rather than from the
 *  documents; no clause numbers are cited because none were verified.
 */
Status_Errno_DDC
i2c_detect_x37(int fd, char * driver) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fd=%d - %s, driver=%s", fd, filename_for_fd_t(fd), driver);

#ifdef EXPLORATORY
   Status_Errno_DDC rc0 = probe_i2c_quick(fd, 0x37, /*write*/ true);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "probe_i2c_quick() 0x%02x  write returned %d", 0x37,rc0);

   rc0 = probe_i2c_quick(fd, 0x37, /*write*/ false);
   DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "probe_i2c_quick 0x%02x read returned %d", 0x37, rc0);
#endif

#ifdef EXPLORATORY
   explore_smbus_quick(fd);
   explore_smbus_probes(fd, driver);
#endif


   Status_Errno_DDC  rc = 0;
   Byte readbuf[1];

   // retry once for -EBUSY
   for (int loopctr = 0; loopctr < 2 ; loopctr++) {
      rc = invoke_i2c_reader(fd, 0x37, false, 1, readbuf);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                     "invoke_i2c_reader() for slave address x37 returned %s", psc_name_code(rc));

      if (rc != 0) {
         Byte writebuf = 0x00;
         // rc = invoke_i2c_writer(fd, 0x37, 1, &writebuf);
         rc = invoke_i2c_writer(fd, 0x37, 0, &writebuf);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                         "invoke_i2c_writer() for slave address x37 returned %s", psc_name_code(rc));
      }

      if (rc == 0 || rc != -EBUSY) {
         break;
      }

      DUAL_MSGX(debug, DDCA_SYSLOG_WARNING, TRACE_GROUP,
               "unexpected -EBUSY reading from or writing to x37");
   }

   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc,"");
   return rc;
}


Status_Errno_DDC
i2c_check_x37_old(int fd, char * driver) {
   bool debug = false;
   DBGTRC_STARTING(debug, TRACE_GROUP, "fd=%d - %s, driver=%s", fd, filename_for_fd_t(fd), driver);

   // Quirks
   // - i2c_set_addr() Causes screen corruption on Dell XPS 13, which has a QHD+ eDP screen
   //   avoided by never calling this function for an eDP screen
   // - Dell P2715Q does not respond to single byte read, but does respond to
   //   a write (7/2018), so this function checks both
   Status_Errno_DDC rc = -1;
   int max_tries =  DETECT_X37_MAX_TRIES;
   int poll_wait_millisec = DETECT_X37_NORMAL_RETRY_MS;
   int loopctr;
   for (loopctr = 0; loopctr < max_tries && rc != 0; loopctr++) {  // retries seem to give no benefit
      if (loopctr > 0) {
         DBGTRC_NOPREFIX(debug, DDCA_TRC_NONE, "driver=%s, sleeping for %d millisec",
                                driver, poll_wait_millisec);
         SLEEP_MILLIS_WITH_SYSLOG(poll_wait_millisec, "Extra x37 sleep");
      }

      // regard either a successful write() or a read() as indication slave address is valid
      Byte writebuf = 0x00;
      rc = invoke_i2c_writer(fd, 0x37, 1, &writebuf);
      // rc = invoke_i2c_writer(fd, 0x37, 0, &writebuf);
      DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                   "invoke_i2c_writer() for slave address x37 returned %s", psc_name_code(rc));
      if (rc != 0) {
         Byte    readbuf[4];  //  4 byte buffer
         rc = invoke_i2c_reader(fd, 0x37, false, 4, readbuf);
         // rc = invoke_i2c_reader(fd, 0x37, false, 1, readbuf);
         DBGTRC_NOPREFIX(debug, TRACE_GROUP,
                   "invoke_i2c_reader() for slave address x37 returned %s", psc_name_code(rc));
      }

      if (rc == -EBUSY) {
         DUAL_MSGXV(debug, DDCA_SYSLOG_WARNING, TRACE_GROUP, "X37 detection encountered EBUSY error");
         max_tries = DETECT_X37_MAX_TRIES + 2;
      }

   }

   if (rc == 0 && loopctr > 1) {
      DUAL_MSGXV(debug, DDCA_SYSLOG_WARNING, TRACE_GROUP, "X37 detection succeeded on try %d", loopctr);
   }


   DBGTRC_RET_DDCRC(debug, TRACE_GROUP, rc,"loopctr=%d", loopctr);
   return rc;
}

void init_i2c_x37() {
   RTTI_ADD_FUNC(i2c_detect_x37);
   RTTI_ADD_FUNC(i2c_check_x37_old);
}
