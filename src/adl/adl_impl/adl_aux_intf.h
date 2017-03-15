/* adl_aux_intf.h
 */

/** \file
 * Functions in this file were originally part of adl_inf.c,
 * but with code refactoring are now only called from tests.
 */

/*
 * <copyright>
 * Copyright (C) 2014-2016 Sanford Rockowitz <rockowitz@minsoft.com>
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

#ifndef ADL_AUX_INTF_H_
#define ADL_AUX_INTF_H_

#include "base/core.h"
#include "base/status_code_mgt.h"


Base_Status_ADL adl_ddc_write_only_with_retry(
      int     iAdapterIndex,
      int     iDisplayIndex,
      Byte *  pSendMsgBuf,
      int     sendMsgLen);

Base_Status_ADL adl_ddc_write_read_with_retry(
      int     iAdapterIndex,
      int     iDisplayIndex,
      Byte *  pSendMsgBuf,
      int     sendMsgLen,
      Byte *  pRcvMsgBuf,
      int *   pRcvBytect);

Base_Status_ADL adl_ddc_get_vcp(int iAdapterIndex, int iDisplayIndex, Byte vcp_feature_code, bool onecall);
Base_Status_ADL adl_ddc_set_vcp(int iAdapterIndex, int iDisplayIndex, Byte vcp_feature_code, int newval);

#endif /* ADL_AUX_INTF_H_ */
