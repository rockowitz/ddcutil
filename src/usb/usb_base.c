/** \file usb_base.c
 *
 * Functions that open and close USB HID devices, and that wrap
 * hiddev ioctl() calls.
 */

// Copyright (C) 2014-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/hiddev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "util/string_util.h"

#include "usb_util/hiddev_util.h"

#include "base/core.h"
#include "base/execution_stats.h"
#include "base/linux_errno.h"
#include "base/rtti.h"

#include "usb/usb_base.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_USB;

// In keeping with the style of Linux USB code, this file prefers
// "struct xxx {}" to "typedef struct {} xxx"


//
// Basic USB HID Device Operations
//

/** Open a USB device
 *
 *  @param  hiddev_devname
 *  @param  calloptions   option flags, checks CALLOPT_RD_ONLY, CALLOPT_ERR_MSG
 *  @return file descriptor (>0) if success, -errno if failure
 */
int
usb_open_hiddev_device(
      char *       hiddev_devname,
      Call_Options calloptions)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "hiddev_devname=%s, calloptions=0x%02x (%s)",
                 hiddev_devname, calloptions, interpret_call_options_t(calloptions) );

   int  file;
   int mode = (calloptions & CALLOPT_RDONLY) ? O_RDONLY : O_RDWR;

   RECORD_IO_EVENT(
         -1,
         IE_OPEN,
         ( file = open(hiddev_devname, mode) )
         );
   // per man open:
   // returns file descriptor if successful
   // -1 if error, and errno is set

   if (file < 0) {
      int errsv = errno;
      if (calloptions & CALLOPT_ERR_MSG)
         f0printf(ferr(), "Open failed for %s: errno=%s\n", hiddev_devname, linux_errno_desc(errsv));
      file = -errsv;
   }
   DBGMSF(debug, "open() finished, file=%d", file);


   DBGTRC(debug, TRACE_GROUP, "Returning file descriptor: %d", file);
   return file;
}


/** Closes an open USB device.
 *
 *  @param fd     file descriptor for open hiddev device
 *  @param device_fn  for use in msgs, ok if NULL
 *  @param calloptions CALLOPT_ERR_MSG recognized
 *  @return -errno if close fails
 */
Status_Errno
usb_close_device(
      int           fd,
      char *        device_fn,
      Call_Options  calloptions)
{
   bool debug = false;
   DBGMSF(debug, "Starting. fd=%d, device_fn=%s, calloptions=0x%02x", fd, device_fn, calloptions);

   errno = 0;
   int rc = 0;
   RECORD_IO_EVENT(fd, IE_CLOSE, ( rc = close(fd) ) );
   int errsv = errno;
   assert(rc<=0);
   if (rc < 0) {
      // EBADF  fd isn't a valid open file descriptor
      // EINTR  close() interrupted by a signal
      // EIO    I/O error
      char workbuf[300];
      if (device_fn)
         snprintf(workbuf, 300,
                  "Close failed for USB device %s. errno=%s",
                  device_fn, linux_errno_desc(errsv));
      else
         snprintf(workbuf, 300,
                  "USB device close failed. errno=%s",
                  linux_errno_desc(errsv));
      if (calloptions & CALLOPT_ERR_MSG)
         fprintf(stderr, "%s\n", workbuf);

      rc = -errsv;
   }
   assert(rc <= 0);
   return rc;
}


//
// Wrapper hiddev ioctl calls
//

Status_Errno
hiddev_get_device_info(
      int                      fd,
      struct hiddev_devinfo *  dev_info,
      Byte                     calloptions)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "Starting.");
   assert(dev_info);

   int rc = ioctl(fd, HIDIOCGDEVINFO, dev_info);
   if (rc != 0) {
      int errsv = errno;
      if ( (calloptions & CALLOPT_ERR_MSG) || debug)
         REPORT_IOCTL_ERROR("HIDIOCGDEVINFO", errsv);
      rc = -errsv;
  }

  assert(rc <= 0);
  DBGTRC(debug, TRACE_GROUP, "Done.     Returning: %s", psc_desc(rc));
  return rc;
}


Status_Errno
hiddev_get_report_info(int fd, struct hiddev_report_info * rinfo, Byte calloptions)
{
   assert(rinfo);

   int rc = ioctl(fd, HIDIOCGREPORTINFO, rinfo);
   if (rc < -1) {     // -1 means no more reports
      int errsv = errno;
      if (calloptions & CALLOPT_ERR_MSG)
         REPORT_IOCTL_ERROR("HIDIOCGREPORTINFO", errsv);
      rc = -errsv;
  }

  return rc;
}


Status_Errno
hiddev_get_field_info(int fd, struct hiddev_field_info * finfo, Byte calloptions)
{
   int saved_field_index = finfo->field_index;
   int rc = ioctl(fd, HIDIOCGFIELDINFO, finfo);
   if (rc != 0) {
      int errsv = errno;
      if (calloptions & CALLOPT_ERR_MSG)
         REPORT_IOCTL_ERROR("HIDIOCGFIELDINFO", errsv);
   }
   assert(rc == 0);
   if (finfo->field_index != saved_field_index && (calloptions & CALLOPT_WARN_FINDEX)) {
      printf("(%s) !!! ioctl(HIDIOCGFIELDINFO) changed field_index from %d to %d\n",
             __func__, saved_field_index, finfo->field_index);
      printf("(%s) finfo.maxusage=%d\n",
             __func__,  finfo->maxusage);
   }
   return rc;
}


Status_Errno
hiddev_get_usage_code(int fd, struct hiddev_usage_ref * uref, Byte calloptions)
{
   int rc = ioctl(fd, HIDIOCGUCODE, uref);    // Fills in usage code
   if (rc != 0) {
      int errsv = errno;
      if (calloptions & CALLOPT_ERR_MSG)
         REPORT_IOCTL_ERROR("HIDIOCGUCODE", errsv);
      rc = -errsv;
   }
   return rc;
}


Status_Errno
hiddev_get_usage_value(int fd, struct hiddev_usage_ref * uref, Byte calloptions)
{
   int rc = ioctl(fd, HIDIOCGUSAGE, uref);
   if (rc != 0) {
      int errsv = errno;
      if (calloptions & CALLOPT_ERR_MSG)
         REPORT_IOCTL_ERROR("HIDIOCGUSAGE", errsv);
      rc = -errsv;
   }
   return rc;
}


Status_Errno
hiddev_get_report(int fd, struct hiddev_report_info * rinfo, Byte calloptions)
{
   int rc = ioctl(fd, HIDIOCGUCODE, rinfo);
   if (rc != 0) {
      int errsv = errno;
      if (calloptions & CALLOPT_ERR_MSG)
         REPORT_IOCTL_ERROR("HIDIOCGREPORT", errsv);
      rc = -errsv;
   }
   return rc;
}


static Bit_Set_32     ignored_hiddevs;
static uint8_t        ignored_vid_pid_ct = 0;
static Vid_Pid_Value* ignored_vid_pids = NULL;


/** Specify /dev/usb/hiddev devices to be ignored, using hiddev bus numbers.
 *
 *  @param ignored_hiddev_flags bits indicate hiddev device numbers to ignore
 */
void
usb_ignore_hiddevs(Bit_Set_32 ignored_hiddevs_flags) {
   bool debug = false;
   ignored_hiddevs = ignored_hiddevs_flags;
   char buf[BIT_SET_32_MAX+1];
   DBGTRC_EXECUTED(debug, TRACE_GROUP, "ignored_hiddevs = 0x%08x = %s",
            ignored_hiddevs, bs32_to_bitstring(ignored_hiddevs, buf, BIT_SET_32_MAX+1));
}


/** Checks if a hiddev device is to ignored, using its /dev/usb/hiddev device number
 *
 *  @param  hiddev_number device number
 *  @return true if device is to be ignored, false if not
 */
bool
usb_is_ignored_hiddev(uint8_t hiddev_number) {
   bool debug = false;
   assert(hiddev_number < BIT_SET_32_MAX);
   bool result = bs32_contains(ignored_hiddevs, hiddev_number);
   DBGTRC_EXECUTED(debug, TRACE_GROUP,
         "hiddev_number=%d, returning %s", hiddev_number, sbool(result));
   return result;
}


/** Specify /dev/usb/hiddev devices to be ignored, using vendor and product ids.
 *
 *  @param ignored_ct  number of devices to ignore
 *  @param ignored     array of vendor_id/product_id values
 *
 *  Each value in **ignored** is specified as a combined 4 byte vendor_id/product_id value.
 */
void
usb_ignore_vid_pid_values(uint8_t ignored_ct, Vid_Pid_Value* ignored) {
   bool debug = false;
   ignored_vid_pid_ct = ignored_ct;
   // explicitly handle ignored==NULL case to avoid coverity warning
   if (ignored_ct > 0) {
      ignored_vid_pids = calloc(ignored_ct, sizeof(uint32_t));
      memcpy(ignored_vid_pids, ignored, ignored_ct*sizeof(uint32_t));
   }
   if (debug || IS_TRACING()) {
      DBGMSG("ignored_vid_pid_ct = %d", ignored_vid_pid_ct);
      for (int ndx = 0; ndx < ignored_vid_pid_ct; ndx++)
         DBGMSG("   ignored_vid_pids[%d] = 0x%08x", ndx, ignored_vid_pids[ndx]);
   }
}


/** Checks if a hiddev device is to ignored, based on its vendor id and product id.
 *
 *  @param  vid  2 byte vendor id
 *  @param  pid  2 byte product id
 *  @return true if device is to be ignored, false if not
 */
bool
usb_is_ignored_vid_pid(uint16_t vid, uint16_t pid) {
   bool debug = false;
   Vid_Pid_Value v = VID_PID_VALUE(vid,pid);
   bool result = usb_is_ignored_vid_pid_value(v);
   DBGTRC_EXECUTED(debug, TRACE_GROUP, "vid=0x%04x, pid=0x%04x, returning: %s", vid, pid, result);
   return result;
}


/** Checks if a hiddev device is to ignored, based on its vendor id and product id.
 *
 *  @param  vidpid  4 byte combined vendor_id/product_id
 *  @return true if device is to be ignored, false if not
 */
bool
usb_is_ignored_vid_pid_value(Vid_Pid_Value vidpid) {
   bool debug = false;
   bool result = false;
   for (int ndx = 0; ndx < ignored_vid_pid_ct; ndx++) {
      if (vidpid == ignored_vid_pids[ndx]) {
         result = true;
         break;
      }
   }
   DBGTRC_EXECUTED(debug, TRACE_GROUP, "vidpid=0x%08x, returning: %s", vidpid, result);
   return result;
}


void
init_usb_base() {
   RTTI_ADD_FUNC(usb_open_hiddev_device);
   RTTI_ADD_FUNC(usb_ignore_hiddevs);
   RTTI_ADD_FUNC(usb_is_ignored_hiddev);
   RTTI_ADD_FUNC(usb_ignore_vid_pid_values);
   RTTI_ADD_FUNC(usb_is_ignored_vid_pid);
   RTTI_ADD_FUNC(usb_is_ignored_vid_pid_value);
}


void
terminate_usb_base() {
   free(ignored_vid_pids);
}
