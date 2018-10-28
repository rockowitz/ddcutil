/** \f ddc_async.h
 *
 *  Experimental async code
 */

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDC_ASYNC_H_
#define DDC_ASYNC_H_

#include "public/ddcutil_types.h"
#include "private/ddcutil_types_private.h"

#include "util/coredefs.h"
#include "util/error_info.h"

#include "base/displays.h"

Error_Info *
start_get_vcp_value(
       Display_Handle *          dh,
       Byte                      feature_code,
       DDCA_Vcp_Value_Type       call_type,
       DDCA_Notification_Func     callback_func);

#endif /* DDC_ASYNC_H_ */
