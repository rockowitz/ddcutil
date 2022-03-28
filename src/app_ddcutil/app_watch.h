/** @file app_watch.h
 *  Implement the WATCH command
 */

 //Copyright (C) 2014-2022 Sanford Rockowitz <rockowitz@minsoft.com>
 // SPDX-License-Identifier: GPL-2.0-or-later

#ifndef APP_WATCH_H_
#define APP_WATCH_H_

#include "base/displays.h"

void
app_read_changes_forever(
      Display_Handle *      dh,
      bool                  force_no_fifo);

void
init_app_watch();
#endif /* APP_WATCH_H_ */
