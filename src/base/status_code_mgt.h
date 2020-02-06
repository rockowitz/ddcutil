/** \file status_code_mgt.h
 *
 *  Status Code Management
 */
// Copyright (C) 2014-2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef STATUS_CODE_MGT_H_
#define STATUS_CODE_MGT_H_

/** \cond */
#include <stdbool.h>
/** \endcond */


#include "public/ddcutil_status_codes.h"

// Called from the mainline to perform initialization
void init_status_code_mgt();

/** Describes one status code.
 *
 * @remark
 * Code could be simplified by using a #Value_Name_Title table instead,
 * but left as is because we might want to add additional fields.
 */
typedef
struct {
   int    code;
   char * name;
   char * description;
   // idea: add flags for NOT_AN_ERROR, DERIVED ?
} Status_Code_Info;

// debugging function:
void report_status_code_info(Status_Code_Info * pdesc);


// For distinguishing types of return codes.
// C does not enforce type checking, but useful for documentation
// trying different styles for readability, consistency w standards

typedef int Status_Errno;          ///< negative Linux errno values
typedef int Status_DDC;            ///< DDC specific status codes
typedef int Status_Errno_DDC;      ///< union(Status_Errno,Status_DDC)
typedef int Base_Status_ADL;       ///< unmodulated ADL status codes
typedef int Modulated_Status_ADL;  ///< modulated ADL status codes
typedef int Public_Status_Code;    ///< union(Status_Errno, Status_DDC, Modulated_Status_ADL)

/** Pointer to function that finds the #Status_Code_Info for a status code
 * @param rc status code
 * @return ponter to #Status_Code_Info for the code, NULL if not found
 * */
typedef
Status_Code_Info * (*Retcode_Description_Finder)(int rc);


/** Pointer to a function that converts a symbolic status code name
 *  to its integer value
 *
 * \param name status code symbolic name
 * @param p_number where to return status code
 * @return true if conversion succeeded, false if name not found
 */
typedef
bool (*Retcode_Number_Finder)(const char * name, int * p_number);

//
// Status codes ranges
//
// #define RCRANGE_BASE_START      0
// #define RCRANGE_BASE_MAX      999
// #define RCRANGE_ERRNO_START  1000
#define RCRANGE_ERRNO_START     0
#define RCRANGE_ERRNO_MAX    1999
#define RCRANGE_ADL_START    2000
#define RCRANGE_ADL_MAX      2999
// #define RCRANGE_DDC_START    3000
#define RCRANGE_DDC_MAX      3999

/** Status code range identifiers
 *
 * @remark
 * - must be kept consistent with table in status_code_mgt.c
 */
typedef enum {
 //       RR_BASE,     ///< indicates unmodulated status code
          RR_ERRNO,    ///< range id for Linux errno values
          RR_ADL,      ///< range id for modulated ADL error codes
          RR_DDC       ///< range id for **ddcutil**-specific error codes
} Retcode_Range_Id;

int modulate_rc(  int unmodulated_rc, Retcode_Range_Id range_id);
int demodulate_rc(int   modulated_rc, Retcode_Range_Id range_id);
Retcode_Range_Id get_modulation(int rc);
// int demodulate_any_rc(int modulated_rc);   // unimplemented

Status_Code_Info * find_status_code_info(Public_Status_Code status_code);

// Return status code description and name.  Do not free after use.
char * psc_desc(Public_Status_Code rc);
char * psc_name(Public_Status_Code status_code);

bool status_name_to_unmodulated_number(
        const char *         status_code_name,
        int *                p_error_number);
bool status_name_to_modulated_number(
        const char *         status_code_name,
        Public_Status_Code * p_error_number);

#ifdef FUTURE
// Future:
bool status_code_name_to_psc_number(
        const char *         status_code_name,
        Public_Status_Code * p_error_number);
#endif

#endif /* STATUS_CODE_MGT_H_ */
