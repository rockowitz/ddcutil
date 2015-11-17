/*
 * adl_from_sample.h
 *
 *  Created on: Jul 27, 2014
 *      Author: rock
 */

#ifndef ADL_FROM_SAMPLE_H_
#define ADL_FROM_SAMPLE_H_

#include <adl/adl_intf.h>
#include <adl/adl_friendly.h>

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
