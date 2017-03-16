/* status_code_mgt.h
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
 * Status Code Management
 */

#ifndef STATUS_CODE_MGT_H_
#define STATUS_CODE_MGT_H_

/** \cond */
#include <stdbool.h>
/** \endcond */


// Called from the mainline to perform initialization
void init_status_code_mgt();

/** Describes one status code.
 *
 * @remark
 * Code could be simplified by using a #Value_Name_Title table
 * instead, but left as is because we might want to add additional
 * fields.
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


#ifdef OLD
Status_Errno_DDC modulate_base_errno_ddc_to_global(Status_Errno_DDC rc);
#endif

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
#define RCRANGE_DDC_START    3000
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

#ifdef OLD
void register_retcode_desc_finder(
        Retcode_Range_Id           id,
        Retcode_Description_Finder finder_func,
        bool                       finder_arg_is_modulated);
#endif

int modulate_rc(  int unmodulated_rc, Retcode_Range_Id range_id);
int demodulate_rc(int   modulated_rc, Retcode_Range_Id range_id);
Retcode_Range_Id get_modulation(int rc);
// int demodulate_any_rc(int modulated_rc);   // unimplemented

#ifdef OLD
Public_Status_Code global_to_public_status_code(Status_Errno_DDC gsc);
Status_Errno_DDC public_to_Base_Status_Errno_DDC(Public_Status_Code);
#endif

Status_Code_Info * find_status_code_info(Public_Status_Code status_code);

// Returns status code description:
#ifdef OLD
char * gsc_desc(Status_Errno_DDC rc);   // must be freed after use
#endif
char * psc_desc(Public_Status_Code rc);
#ifdef OLD
char * gsc_name(Status_Errno_DDC status_code);   // do not free after use
#endif

char * psc_name(Public_Status_Code status_code);   // do not free after us


bool status_name_to_unmodulated_number(const char * status_code_name, int * p_error_number);
bool status_name_to_modulated_number(const char * status_code_name, Public_Status_Code * p_error_number);

// new    ???
bool status_code_name_to_psc_number(
      const char * status_code_name,
      Public_Status_Code * p_error_number);

#endif /* STATUS_CODE_MGT_H_ */
