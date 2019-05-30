/** @file usb_hid_common.c
 *
 *  Functions that are common to the wrappers for multiple USB HID
 *  packages such as libusb, hiddev
 */

// Copyright (C) 2014-2019 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/** \cond */
#include <stdio.h>

#include "util/string_util.h"
/** \endcond */

#include "usb_util/usb_hid_common.h"


/* Return's the name of a collection type in a HID report descriptor.
 *
 * Per USB HID Specification v1.11, Section 6.2.2.6
 */
const char * collection_type_name(uint8_t collection_type) {
   static char * collection_type_names[] = {
         "Physical",        // 0
         "Application",     // 1
         "Logical",         // 2
         "Report",          // 3
         "Named Array",     // 4
         "Usage Switch",    // 5
         "Usage_Modifier",  // 6
   };
   char * result = NULL;

   if (collection_type < 7)
      result = collection_type_names[collection_type];
   else if (collection_type & 0x80)
      result = "Vendor defined";
   else
      result = "Reserved for future use.";
   return result;
}


/* Check for specific USB devices that should be treated as
 * monitors, even though the normal monitor check fails.
 *
 * This is a hack.
 *
 * Arguments:
 *   vid      USB vendor id
 *   pid      USB product id
 *
 * Returns    true/false
 */
bool force_hid_monitor_by_vid_pid(int16_t vid, int16_t pid) {
   struct vid_pid {
      uint16_t   vid;
      uint16_t   pid;
   };

   bool debug = false;
   if (debug)
      printf("(%s) Starting. vid=0x%04x, pid=0x%04x\n", __func__, vid, pid);
   bool result = false;


   struct vid_pid exceptions[] = {
            {0x0424, 0x3328},    // Std Micrososystems USB HID I2C - HP LP2480
            {0x056d, 0x0002},    // Eizo,      HID Monitor Controls
            {0x0451, 0xca01},    // Texas Instruments USB to I2C Solution  - what is this

            // NEC monitors
            {0x0409, 0x040d},    // P232W
            {0x0409, 0x02b7},    // P241W
            {0x0409, 0x042c},    // P242W
            {0x0409, 0x02bb},    // PA231W
            {0x0409, 0x02b8},    // PA241W   (seen at RIT)
            {0x0409, 0x042d},    // PA242W
            {0x0409, 0x02b9},    // PA271W
            {0x0409, 0x042e},    // PA272W
            {0x0409, 0x02ba},    // PA301W
            {0x0409, 0x042f},    // PA302W
            {0x0409, 0x02bc},    // MD301C4
            {0x0409, 0x040a},    // MD211G3
            {0x0409, 0x040b},    // MD211C3
            {0x0409, 0x040c},    // MD211C2
            {0x0409, 0x042b},    // MD242C2
            {0x0409, 0x044f},    // EA244UHD
            {0x0409, 0x042b},    // EA304WMi
            {0x0409, 0x046b},    // PA322UHD
            {0x0409, 0x047d},    // X841UHD
            {0x0409, 0x04ac},    // X981UHD
            {0x0409, 0x04ad},    // X651UHD
            {0x0409, 0x046c},    // MD322C8
            {0x0409, 0x04Ae},    // P212
            {0x0409, 0x050c},    // PA322UHD2

            // additional values from usb.ids
            {0x0419, 0x8002},    // Samsung,   Syncmaster HID Monitor Control
            {0x0452, 0x0021},    // Misubishi, HID Monitor Controls
            {0x04a6, 0x0181},    // Nokia,     HID Monitor Controls
            {0x04ca, 0x1766},    // Lite-on,   HID Monitor Controls
      };
   const int vid_pid_ct = sizeof(exceptions)/sizeof(struct vid_pid);

   for (int ndx = 0; ndx < vid_pid_ct && !result; ndx++) {
      if (vid == exceptions[ndx].vid) {
         // there used to be some buggy code here looking at case of exceptions[ndx].pid == 0,  why?
         if ( pid == exceptions[ndx].pid) {
            result = true;
            // if (debug)
               printf("(%s) Matched exception vid=0x%04x, pid=0x%04x\n", __func__,
                      exceptions[ndx].vid, exceptions[ndx].pid);
         }
      }
   }

   if (debug)
      printf("(%s) vid=0x%04x, pid=0x%04x, returning: %s\n", __func__, vid, pid, bool_repr(result));
   return result;
}


// This function was introduced because of a user bug report that display detection
// caused the trackpoint on a Lenovo SK-8855 Thinkpad keyboard with pointing device
// to stop working.  It appears not to have solved the problem. (The reporting user ceased
// running test builds.)

/* Check for specific USB devices to not probe.
 *
 * This is a hack.
 *
 * Arguments:
 *   vid      USB vendor id
 *   pid      USB product id
 *
 * Returns    true/false
 */
bool deny_hid_monitor_by_vid_pid(int16_t vid, int16_t pid) {
   struct vid_pid {
      uint16_t   vid;
      uint16_t   pid;
   };

   bool debug = false;
   if (debug)
      printf("(%s) Starting. vid=0x%04x, pid=0x%04x\n", __func__, vid, pid);
   bool result = false;

   struct vid_pid exceptions[] = {
            {0x17ef, 0x6009},    // ThinkPad USB Keyboard with TrackPoint
      };
   const int vid_pid_ct = sizeof(exceptions)/sizeof(struct vid_pid);

   for (int ndx = 0; ndx < vid_pid_ct && !result; ndx++) {
      if (vid == exceptions[ndx].vid) {
         // there used to be some buggy code here looking at case of exceptions[ndx].pid == 0,  why?
         if ( pid == exceptions[ndx].pid) {
            result = true;
            // if (debug)
               printf("(%s) Matched exception vid=0x%04x, pid=0x%04x\n", __func__,
                      exceptions[ndx].vid, exceptions[ndx].pid);
         }
      }
   }

   if (debug)
      printf("(%s) vid=0x%04x, pid=0x%04x, returning: %s\n", __func__, vid, pid, bool_repr(result));
   return result;
}

