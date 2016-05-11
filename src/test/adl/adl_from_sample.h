/* adl_from_sample.h
 *
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

#ifndef ADL_FROM_SAMPLE_H_
#define ADL_FROM_SAMPLE_H_

#include "adl/adl_impl/adl_intf.h"
#include "adl/adl_impl/adl_friendly.h"

typedef unsigned int UINT;
typedef unsigned char UCHAR;


//********************************
// Prototypes
//********************************
int   vGetVcpCommand(UCHAR ucVcp, UINT * ulMaxVal, UINT * ulCurVal, int iAdapterIndex, int iDisplayIndex);
int   vSetVcpCommand(UCHAR ucVcp, UINT ulVal, int iAdapterIndex, int iDisplayIndex);
bool  vGetCapabilitiesCommand(UCHAR ucVcp, int iAdapterIndex, int iDisplayIndex);
int   vWriteI2c(char * lpucSendMsgBuf, int iSendMsgLen, int iAdapterIndex, int iDisplayIndex);
int   vWriteAndReadI2c(char *lpucSendMsgBuf, int iSendMsgLen, char * lpucRecvMsgBuf, int iRecvMsgLen,  int iAdapterIndex, int iDisplayIndex);
// bool  InitADL();
// void  FreeADL();


#endif /* ADL_FROM_SAMPLE_H_ */
