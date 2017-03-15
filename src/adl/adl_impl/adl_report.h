/* adl_report.h
 *
 * Report on data structures in ADL SDK
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

/** \file
 * Reports on data structures in ADL SDK.
 *
 * Used only for development and debugging.
 */
#ifndef ADL_REPORT_H_
#define ADL_REPORT_H_

/** \cond */
#include <stdbool.h>
/** \endcond */

#include "util/report_util.h"

#include "adl/adl_impl/adl_sdk_includes.h"


void report_adl_AdapterInfo(AdapterInfo * pAdapterInfo, int depth);

void report_adl_ADLDisplayID(ADLDisplayID * pADLDisplayID, int depth);

void report_adl_ADLDisplayInfo(ADLDisplayInfo * pADLDisplayInfo, int depth);

void report_adl_ADLDisplayEDIDData(ADLDisplayEDIDData * pEDIDData, int depth);

void report_adl_ADLDDCInfo2( ADLDDCInfo2 * pStruct, bool verbose, int depth);

#endif /* ADL_REPORT_H_ */
