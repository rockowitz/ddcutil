/*
 * status_code_mgt.h
 *
 *  Created on: Nov 3, 2015
 *      Author: rock
 */

#ifndef STATUS_CODE_MGT_H_
#define STATUS_CODE_MGT_H_

#include <stdbool.h>

#include <base/util.h>


// Called from the mainline to perform initialization
void init_status_code_mgt();

typedef
struct {
   int    code;
   char * name;
   char * description;
} Status_Code_Info;

// debugging function:
void report_status_code_info(Status_Code_Info * pdesc);

typedef
Status_Code_Info * (*Retcode_Description_Finder)(int rc);


// For distinguishing types of return codes.
// C does not enforce type checking, but useful for documentation
// trying different styles for readability, consistency w standards
typedef int Global_Status_Code;
// Global_Status_Code subranges:
typedef int Global_Status_ADL;    // subrange of Global_Status_Code containing modulated ADL return codes
typedef int Global_Status_DDC;
typedef int Global_Status_Errno;

// typedef int Rc_Raw_DDC_t;
typedef int Base_Status_ADL;
typedef int Base_Status_Errno;
// would make it simpler for low level I2C functions to be incorporated into sample code
typedef int Base_Status_Errno_DDC;   // union(Base_Status_Errno, Global_Status_DDC)
typedef int Base_Status_ADL_DDC;     // union(Base_Status_ADL, Global_Status_DDC)

Global_Status_Code modulate_base_errno_ddc_to_global(Base_Status_Errno_DDC rc);



//
// Status codes ranges
//
#define RCRANGE_BASE_START      0
#define RCRANGE_BASE_MAX      999
#define RCRANGE_ERRNO_START  1000
#define RCRANGE_ERRNO_MAX    1999
#define RCRANGE_ADL_START    2000
#define RCRANGE_ADL_MAX      2999
#define RCRANGE_DDC_START    3000
#define RCRANGE_DDC_MAX      3999



// DDCRC_DOUBLE_BYTE probably not worth keeping, can only reliably check for
// small subset of DDCRC_PACKET_SIZE, DDCRC_RESPONSE_ENVELOPE, DDCRC_CHECKSUM



// must be kept consistent with table in status_code_mgt.c
// should RR_BASE be in this enum?
typedef enum {RR_BASE, RR_ERRNO, RR_ADL, RR_DDC} Retcode_Range_Id;

// for modules to register the explanation routine for their
// status codes, to avoid circular dependencies of includes

//deprecated
// typedef char * (*Retcode_Explainer)(int rc);


void register_retcode_desc_finder(
        Retcode_Range_Id           id,
        Retcode_Description_Finder finder_func,
        bool                       finder_arg_is_modulated);


int modulate_rc(int unmodulated_rc, Retcode_Range_Id range_id);
int demodulate_rc(int modulated_rc, Retcode_Range_Id range_id);
Retcode_Range_Id get_modulation(int rc);
// int demodulate_any_rc(int modulated_rc);   // unimplemented


// Maintain counts of DDC data errors

void init_status_counts();
Global_Status_Code record_status_code_occurrence(Global_Status_Code rc, const char * caller_name);
#define COUNT_STATUS_CODE(rc) record_status_code_occurrence(rc,__func__)
void show_status_counts();
#ifdef FUTURE
int get_status_code_count(int rc);
#endif



Status_Code_Info * find_global_status_code_description(Global_Status_Code rc);

// Returns status code description:
char * global_status_code_description(Global_Status_Code rc);   // must be freed after use


#endif /* STATUS_CODE_MGT_H_ */
