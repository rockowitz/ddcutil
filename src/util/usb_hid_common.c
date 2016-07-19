/* usb_hid_common.c
 *
 * Functions that are common to the wrappers for multiple USB HID
 * packages such as libusb, hiddev
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


#include <stdio.h>

#include "util/usb_hid_common.h"


struct vid_pid {
   int16_t   vid;
   int16_t   pid;
};


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
   bool debug = true;
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
            if (exceptions[ndx].pid == 0 && pid == exceptions[ndx].pid) {
               result = true;
               if (debug)
                  printf("(%s) Matched exception vid=0x%04x, pid=0x%04x\n", __func__,
                         exceptions[ndx].vid, exceptions[ndx].pid);
            }
         }
      }

   return result;
}
