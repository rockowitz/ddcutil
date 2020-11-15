/** \file usb_base.c
 *
 * Functions that open and close USB HID devices, and that wrap
 * hiddev ioctl() calls.
 */

// Copyright (C) 2014-2020 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include "usb/usb_base.h"


// Trace class for this file
static DDCA_Trace_Group TRACE_GROUP = DDCA_TRC_USB;

// In keeping with the style of Linux USB code, this file prefers
// "struct xxx {}" to "typedef {} xxx"


//
// Basic USB HID Device Operations
//

/* Open a USB device
 *
 * Arguments:
 *   hiddev_devname
 *   calloptions      checks CALLOPT_RDONLY, CALLOPT_ERR_MSG
 *
 * Returns:
 *   file descriptor ( >= 0) if success
 *   -errno if failure
 *
 */
int usb_open_hiddev_device(
      char *       hiddev_devname,
      Call_Options calloptions)
{
   bool debug = false;
   DBGTRC(debug, TRACE_GROUP, "hiddev_devname=%s, calloptions=0x%02x (%s)",
                 hiddev_devname, calloptions, interpret_call_options_t(calloptions) );

   int  file;
   int mode = (calloptions & CALLOPT_RDONLY) ? O_RDONLY : O_RDWR;

   RECORD_IO_EVENT(
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

   if (file >= 0)
   {
      // Solves problem of ddc detect not getting edid unless ddcutil env called first
      int rc = ioctl(file, HIDIOCINITREPORT);
      if (rc < 0) {
         int errsv = errno;
         // call should never fail.  always write an error message
         REPORT_IOCTL_ERROR("HIDIOCGREPORT", errsv);
         close(file);
         file = -errsv;
      }
   }
   DBGTRC(debug, TRACE_GROUP, "Returning file descriptor: %d", file);
   return file;
}


/* Closes an open USB device.
 *
 * Arguments:
 *   fd     file descriptor for open hiddev device
 *   device_fn
 *          if NULL, ignore
 *   failure_action  if true, exit if close fails
 *
 * Returns:
 *    0 if success
 *    -errno if close fails and exit on failure was not specified
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
   RECORD_IO_EVENT(IE_CLOSE, ( rc = close(fd) ) );
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
