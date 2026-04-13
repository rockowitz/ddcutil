/** \file utilrpt.h
 *
 * Report functions for other utility modules.
 *
 * Avoids complex dependencies between report_util.c and other util files.
 */

// Copyright (C) 2014-2026 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef UTILRPT_H_
#define UTILRPT_H_

#include "data_structures.h"

void dbgrpt_buffer(Buffer * buffer, int depth);

#endif /* UTILRPT_H_ */
