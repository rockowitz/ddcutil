/* usb_base.c
 *
 * Functions that open and close USB HID devices, and that wrap
 * hiddev ioctl() calls.
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */


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
static Trace_Group TRACE_GROUP = TRC_USB;

// In keeping with the style of Linux USB code, this module prefers
// "struct xxx {}" to "typedef {} xxx"



//
// Basic USB HID Device Operations
//

/* Open a USB device
 *
 * Arguments:
 *   hiddev_devname
 *   readonly         if true, open read only
 *                    if false, open for reading and writing
 *   emit_error_msg   if true, output message if error
 *
 * Returns:
 *   file descriptor ( > 0) if success
 *   -errno if failure
 *
 */
int usb_open_hiddev_device(char * hiddev_devname, Byte calloptions) {
   bool debug = false;
   DBGMSF(debug, "hiddev_devname=%s, calloptions=0x%02x (%s)",
                 hiddev_devname, calloptions, interpret_call_options(calloptions));

   int  file;
   int mode = (calloptions & CALLOPT_RDONLY) ? O_RDONLY : O_RDWR;

   RECORD_IO_EVENT(
         IE_OPEN,
         ( file = open(hiddev_devname, mode) )
         );
   // per man open:
   // returns file descriptor if successful
   // -1 if error, and errno is set
   int errsv = errno;
   if (file < 0) {
      if (calloptions & CALLOPT_ERR_ABORT)
         TERMINATE_EXECUTION_ON_ERROR(
               "Open failed for %s: errno=%s\n", hiddev_devname, linux_errno_desc(errsv));

      if (calloptions & CALLOPT_ERR_MSG)
         f0printf(FERR, "Open failed for %s: errno=%s\n", hiddev_devname, linux_errno_desc(errsv));
      file = -errno;
   }
   DBGMSF(debug, "open() finished, file=%d", file);

   if (file > 0)
   {
      // Solves problem of ddc detect not getting edid unless ddcutil env called first
      errsv = errno;
      int rc = ioctl(file, HIDIOCINITREPORT);
      if (rc != 0) {
         // call should never fail.  always wrote an error message
         REPORT_IOCTL_ERROR("HIDIOCGREPORT", rc);
         close(file);
         if (calloptions & CALLOPT_ERR_ABORT)
            DDC_ABORT(errsv);
         file = rc;
      }
   }
   DBGMSF(debug, "Returning file descriptor: %d", file);
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
Base_Status_Errno usb_close_device(int fd, char * device_fn, Byte calloptions) {
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

      if (calloptions & CALLOPT_ERR_ABORT)
         TERMINATE_EXECUTION_ON_ERROR(workbuf);

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

Base_Status_Errno
hiddev_get_device_info(
      int                      fd,
      struct hiddev_devinfo *  dev_info,
      Byte                     calloptions) {
   assert(dev_info);

   int rc = ioctl(fd, HIDIOCGDEVINFO, dev_info);
   if (rc != 0) {
      int errsv = errno;
      if (calloptions & CALLOPT_ERR_MSG)
         REPORT_IOCTL_ERROR("HIDIOCGDEVINFO", rc);

      if (calloptions & CALLOPT_ERR_ABORT)
         DDC_ABORT(errsv);
      rc = -errsv;
  }

  assert(rc <= 0);
  return rc;
}


int hiddev_get_report_info(int fd, struct hiddev_report_info * rinfo, Byte calloptions) {
   assert(rinfo);

   int rc = ioctl(fd, HIDIOCGREPORTINFO, rinfo);
   if (rc < -1) {     // -1 means no more reports
      int errsv = errno;
      if (calloptions & CALLOPT_ERR_MSG)
         REPORT_IOCTL_ERROR("HIDIOCGREPORTINFO", rc);

      if (calloptions & CALLOPT_ERR_ABORT)
         DDC_ABORT(errsv);
  }

  return rc;
}


int hiddev_get_field_info(int fd, struct hiddev_field_info * finfo, Byte calloptions) {
   int saved_field_index = finfo->field_index;
   int rc = ioctl(fd, HIDIOCGFIELDINFO, finfo);
   if (rc != 0) {
      int errsv = errno;
      if (calloptions & CALLOPT_ERR_MSG)
         REPORT_IOCTL_ERROR("HIDIOCGFIELDINFO", rc);

      if (calloptions & CALLOPT_ERR_ABORT)
         DDC_ABORT(errsv);
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


int hiddev_get_usage_code(int fd, struct hiddev_usage_ref * uref, Byte calloptions) {
   int rc = ioctl(fd, HIDIOCGUCODE, uref);    // Fills in usage code
   if (rc != 0) {
      int errsv = errno;
      if (calloptions & CALLOPT_ERR_MSG)
         REPORT_IOCTL_ERROR("HIDIOCGUCODE", rc);

      if (calloptions & CALLOPT_ERR_ABORT)
         DDC_ABORT(errsv);
   }

   return rc;
}


int hiddev_get_usage_value(int fd, struct hiddev_usage_ref * uref, Byte calloptions) {
   int rc = ioctl(fd, HIDIOCGUSAGE, uref);
   if (rc != 0) {
      int errsv = errno;
      if (calloptions & CALLOPT_ERR_MSG)
         REPORT_IOCTL_ERROR("HIDIOCGUSAGE", rc);

      if (calloptions & CALLOPT_ERR_ABORT)
         DDC_ABORT(errsv);
   }

   return rc;
}


int hiddev_get_report(int fd, struct hiddev_report_info * rinfo, Byte calloptions) {
   int rc = ioctl(fd, HIDIOCGUCODE, rinfo);
   if (rc != 0) {
      int errsv = errno;
      if (calloptions & CALLOPT_ERR_MSG)
         REPORT_IOCTL_ERROR("HIDIOCGREPORT", rc);

      if (calloptions & CALLOPT_ERR_ABORT)
         DDC_ABORT(errsv);
   }

   return rc;
}
