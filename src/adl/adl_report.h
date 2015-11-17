/*  adl_report.h
 *
 *  Created on: Jul 18, 2014
 *      Author: rock
 *
 *  Report on data structures in ADL SDK
 */

#ifndef ADL_REPORT_H_
#define ADL_REPORT_H_

#include <stdbool.h>

#include <util/report_util.h>

#include <adl/adl_sdk_includes.h>


void report_adl_AdapterInfo(AdapterInfo * pAdapterInfo, int depth);

void report_adl_ADLDisplayID(ADLDisplayID * pADLDisplayID, int depth);

void report_adl_ADLDisplayInfo(ADLDisplayInfo * pADLDisplayInfo, int depth);

void report_adl_ADLDisplayEDIDData(ADLDisplayEDIDData * pEDIDData, int depth);

void report_adl_ADLDDCInfo2( ADLDDCInfo2 * pStruct, bool verbose, int depth);

#endif /* ADL_REPORT_H_ */
