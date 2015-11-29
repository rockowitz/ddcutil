/*
 * adl_services.h
 *
 *  Created on: Oct 30, 2015
 *      Author: rock
 *
 *  Functions in this file were originally part of adl_inf.c,
 *  but with code refactoring are now only called from tests.
 */

#ifndef ADL_AUX_INTF_H_
#define ADL_AUX_INTF_H_

#include <base/status_code_mgt.h>
#include <base/util.h>


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
