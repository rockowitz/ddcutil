/** @file ddcutil_c_api.h
 *
 *  @brief Public C API for ddcutil
 *
 *  Function names in the public C API begin with "ddca_"\n
 *  Status codes begin with "DDCRC_".\n
 *  Typedefs, other constants, etc. begin with "DDCA_".
 */

// Copyright (C) 2014-2025 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DDCUTIL_C_API_H_
#define DDCUTIL_C_API_H_

/** @cond */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
/** @endcond */

#ifdef __cplusplus
extern "C" {
#endif

#include "ddcutil_types.h"


/* Note on report functions.
 *
 * Functions whose names begin with "ddca_report" or "ddca_dbgrpt",
 * e.g. ddca_report_display_ref(), ddca_report_display_info_list(), write
 * formatted reports to (normally) the terminal. Sometimes, these are intended
 * to display data structures for debugging.  Other times, they are used to
 * format output for the ddcutil command line program.
 *
 * The operation of these functions can be tweaked in two ways.
 * - The "depth" parameter is a logical indentation depth. This enables
 *   reports that invoke other reports to indent the subreports
 *   sensibly.  At the level of the ddcutil_c_api(), one unit of
 *   logical indentation depth translates to 3 spaces.
 * - The destination of reports is normally the STDOUT device.  This can
 *   be changed by calling set_fout().
 */

/* Note on convenience functions.
 *
 * Many functions in this API are "convenience" functions. They perform tasks
 * that could be executed entirely on the client.  By providing access to the
 * server's implementation of these tasks, they exist to avoid having to
 * recreate such code on the client. In some cases, such as "free" functions,
 * they relieve the client programmer of having to consider implementation details.
 */

//
// Library build information
//

/**
 * Returns the ddcutil version as a struct of 3 8 bit integers.
 *
 * @return version numbers
 */
DDCA_Ddcutil_Version_Spec
ddca_ddcutil_version(void);

/**
 * Returns the ddcutil version as a string in the form "major.minor.micro".
 *
 * @return version string (caller must not free)
 */
const char *
ddca_ddcutil_version_string(void);

/** Returns the full ddcutil version as a string, possibly with a suffix,
 *
 * @return string in form "1.3.0" or "1.3.0-dev" (caller must not free)
 *
 * @since 1.2.0
 */
const char *
ddca_ddcutil_extended_version_string(void);

/** Queries the options with which the **ddcutil** library was built.
 *
 * @return  flags byte
 */
DDCA_Build_Option_Flags
ddca_build_options(void);

 /** Returns the fully qualified name of the shared library file
  *
  *  @return file name
  *
  *  @since 2.0.0
  */
 const char *
 ddca_libddcutil_filename(void);


 //
 // Error Detail
 //

 /** Gets a copy of the detailed error information for the previous
  *  API call, if the call supports detailed error information (only
  *  some do).
  *
  *  @return  copy of detailed error information (user must free)
  *
  *  @since 0.9.0
  */
 DDCA_Error_Detail *
 ddca_get_error_detail(void);

 /** Frees a detailed error information record
  *
  *  @param[in]  ddca_erec  error information to free
  *
  *  @remark
  *  This is a convenience function.
  *
  *  @since 0.9.0
  */
 void
 ddca_free_error_detail(
       DDCA_Error_Detail * ddca_erec);

 /** Issues a detailed report of a #DDCA_Error_Detail instance.
  *
  *  @param[in] ddca_erec  error information record
  *  @param[in] depth      logical indentation depth
  *
  *  @remark
  *  This is a convenience function.
  */
 void
 ddca_report_error_detail(
       DDCA_Error_Detail * ddca_erec,
       int                 depth);


//
// Status Codes
//

/** Returns the symbolic name for a ddcutil status code
 *
 * @param[in] status_code numeric status code
 * @return    symbolic name, e.g. EBUSY, DDCRC_INVALID_DATA
 * @retval    NULL if unrecognized code
 *
 * @remark
 * The returned value is a pointer into internal persistent
 * data structures and should not be freed by the caller.
 */
const char *
ddca_rc_name(
      DDCA_Status status_code);

/** Returns a description of a ddcutil status code
 *
 * @param[in] status_code numeric status code
 * @return    explanation of status code, e.g. "device or resource busy"
 * @retval    "unknown status code" if unrecognized code
 *
 * @remark
 * The returned value is a pointer into internal persistent
 * data structures and should not be free'd by the caller.
 */
const char *
ddca_rc_desc(
      DDCA_Status status_code);


//
// Initialization
//

/** Performs library initialization.
 *
 *  @deprecated
 *  Use ddca_init2()
 *
 *  @param[in]  library_options  string of **libddcutil** options
 *  @param[in]  syslog_level     severity cutoff for system log
 *  @param[in]  opts             option flags
 *  @return status code
 *
 * Unless flag DDC_INIT_OPTIONS_DISABLE_CONFIG_FILE is set in **opts**,
 * libddcutil options are read from the ddcutil configuration file.
 * These are combined with any options passed in string **libopts**
 * and then processed.
 *
 * If the returned status code is other than **DDCRC_OK**, a detailed
 * error report can be obtained using #ddca_get_error_detail().
 *
 * @since 2.0.0
 */
DDCA_Status
ddca_init(const char *      libopts,
          DDCA_Syslog_Level syslog_level,
          DDCA_Init_Options opts);


/** Performs library initialization.
 *
 *  @param[in]  library_options  string of **libddcutil** options
 *  @param[in]  syslog_level     severity cutoff for system log
 *  @param[in]  opts             option flags
 *  @param[out] infomsg_loc      if non-null, return null terminated list of
 *                               informational msgs here
 *  @return status code
 *
 * Unless flag DDC_INIT_OPTIONS_DISABLE_CONFIG_FILE is set in **opts**,
 * libddcutil options are read from the ddcutil configuration file.
 * These are combined with any options passed in string **libopts**
 * and then processed.
 *
 * If the returned status code is other than **DDCRC_OK**, a detailed
 * error report can be obtained using #ddca_get_error_detail().
 *
 * The caller is responsible for freeing the null terminated list of
 * messages returned in infomsg_loc.
 *
 * @since 2.1.0
 */
DDCA_Status
ddca_init2(const char *     libopts,
          DDCA_Syslog_Level syslog_level_arg,
          DDCA_Init_Options opts,
          char***           infomsg_loc);


//
// Global Settings
//

/** Controls whether VCP values are read after being set.
 *
 * @param[in] onoff true/false
 * @return  prior value
 *
 * @remark This setting is thread-specific.
 */
bool
ddca_enable_verify(
      bool onoff);

/** Query whether VCP values are read after being set.
 * @retval true values are verified after being set
 * @retval false values are not verified
 *
 * @remark This setting is thread-specific.
 */
bool
ddca_is_verify_enabled(void);


//
// Performance
//

/** Sets the sleep multiplier factor for the open display on current thread.
 *
 *  The semantics of this function has changed. Prior to release 1.5,
 *  this function set the sleep multiplier for the current thread.
 *  As of release 2.0, it sets the sleep multiplier for open display
 *  (if any) on the current thread.
 *
 *  @param[in]  multiplier, must be >= 0 and <= 10
 *  @return     old multiplier, -1.0f if invalid multiplier specified, or no display open
 *
 *  @deprecated
 *  This function provides backwards compatibility with applications
 *  written for libddcutil release 1.x. It should not be used in new applications.
 */
__attribute__ ((deprecated)) double
ddca_set_sleep_multiplier(double multiplier);

/** Gets the sleep multiplier for the open display on the current thread
 *
 *  As of release 2.0, the semantics of this function changed.
 *  See #ddca_set_sleep_multiplier().
 *
 *  @return  sleep multiplier, -1.0f if no display open on current thread
 */
__attribute__ ((deprecated)) double
ddca_get_sleep_multiplier();


/** Sets an explicit sleep multiplier factor for the specified display.
 *  If set, it takes precedence over any other sleep multiplier calculation.
 *
 *  @param[in]  dref        display reference
 *  @param[in]  multiplier  must be >= 0 and <= 10
 *  @retval     DDCRC_OK
 *  @retval     DDCRC_ARG   invalid display reference or multiplier value
 */
DDCA_Status
ddca_set_display_sleep_multiplier(
      DDCA_Display_Ref       dref,
      DDCA_Sleep_Multiplier  multiplier);

/** Gets the current effective sleep multiplier for the specified display.
 *
 *  This value can vary if dynamic sleep adjustment is active.
 *
 *  @param   dref           display reference
 *  @param   multiplier_loc where to return answer
 *  @retval  DDCRC_OK
 *  @retval  DDCRC_ARG      invalid display reference
 */
DDCA_Status
ddca_get_current_display_sleep_multiplier(
      DDCA_Display_Ref        dref,
      DDCA_Sleep_Multiplier*  multiplier_loc);


/** Controls whether dynamic sleep adjustment is enabled.
 *  This is a global setting that applies to all displays.
 *
 *  @param  onoff
 *  @return previous setting
 *
 *  @since 2.1.0
 */
bool
ddca_enable_dynamic_sleep(bool onoff);


/** Reports whether dynamic sleep adjustment is enabled.
 *
 *  @return current setting
 *
 *  @since 2.1.0
 */
bool
ddca_is_dynamic_sleep_enabled();


//
// Output Redirection
//

/** Redirects output on the current thread that normally would go to **stdout**  */
void
ddca_set_fout(
    FILE * fout);   /**< where to write normal messages, if NULL, suppress output */

/** Redirects output on the current thread that normally goes to **stdout** back to **stdout** */
void
ddca_set_fout_to_default(void);

/** Redirects output on the current thread that normally would go to **stderr**  */
void
ddca_set_ferr(
    FILE * ferr);   /**< where to write error messages, If NULL, suppress output */

/** Redirects output on the current thread that normally goes to **stderr** back to **stderr** */
void
ddca_set_ferr_to_default(void);


//
// Utility functions for capturing output by redirecting
// to an in-memory buffer.
//

/** Begins capture of **stdout** and optionally **stderr** output on the
 *  current thread to a thread-specific in-memory buffer.
 *
 *  @param[in] flags option flags
 *
 *  @note  If output is already being captured, this function has no effect.
 *  @since 0.9.0
 */
void
ddca_start_capture(
      DDCA_Capture_Option_Flags flags);

/** Ends capture of **stdout** output and returns the contents of the
 *  in-memory buffer.
 *
 *  Upon termination, normal thread output is directed to **stdout**.
 *  If error output was also being captured, error output is redirected
 *  to **stderr**.
 *
 *  @return captured output as a string, caller responsible for freeing
 *
 *  @note
 *  If output is not currently being captured, returns a 0 length string.
 *
 *  @note  Writes messages to actual **stderr** in case of error.
 *  @since 0.9.0
 */
char *
ddca_end_capture(void);


//
// Message Control
//

/** Gets the current output level for the current thread
 *  @return      output level
 */
DDCA_Output_Level
ddca_get_output_level(void);

/** Sets the output level for the current thread
 *
 *  @param[in]      new output level
 *  @return         prior output level
 */
DDCA_Output_Level
ddca_set_output_level(
      DDCA_Output_Level newval);

/** Gets the name of an output level
 *
 *  @param[in]  val  output level id
 *  @return     output level name (do not free)
 *
 *  @remark
 *  This is a convenience function.
 */
char *
ddca_output_level_name(
      DDCA_Output_Level val);   /**< output level id */

/** Given an external syslog level name returns the syslog level id.
 *
 *  @param[in]  name    e.g. ERROR
 *  @return syslog level id, DDCA_SYSLOG_NOT FOUND if invalid name
 *
 *  @remark
 *  This is a convenience function.
 *
 * @since 2.0.0
 */
DDCA_Syslog_Level ddca_syslog_level_from_name(const char * name);


//
// Statistics and Diagnostics
//

/** Resets all **ddcutil** statistics */
void
ddca_reset_stats(void);

/** Show execution statistics.
 *
 *  @param[in] stats  bitflags of statistics types to show
 *  @param[in] include_per_display_data include per display detail
 *  @param[in] depth  logical indentation depth
 *
 *  @remark
 *  Prior to version 2.0.0, the second parm was named **include_per_thread_data**
 *  and caused per-thread data to be reported.  Most of this data is now
 *  maintained on a per-display basis
 */
void
ddca_show_stats(
      DDCA_Stats_Type stats,
      bool            include_per_display_data,
      int             depth);

// TODO: Add functions to get stats

/** Report display locks.
 *
 *  @param[in] depth  logical indentation depth
 *
 *  @since 2.0.0
 */
void
ddca_report_locks(
      int            depth);


//
// Display Detection
//

/** Gets display references list for all detected displays.
 *
 *  @param[in]  include_invalid_displays if true, displays that do not support DDC are included
 *  @param[out] drefs_loc where to return pointer to null-terminated array of #DDCA_Display_Ref
 *  @retval     0  always succeeds
 *
 *  @since 1.2.0
 */
DDCA_Status
ddca_get_display_refs(
      bool                include_invalid_displays,
      DDCA_Display_Ref**  drefs_loc);

/** @deprecated use #ddca_get_get_display_info2
 *  Gets publicly visible information about a display reference
 *
 *  The returned struct can simply be free()'d by the client.
 *
 *  @param[in]  ddca_dref display reference
 *  @param[out] dinfo_loc where to return pointer to newly allocated #DDCA_Display_Info
 *  @retval DDCRC_OK  no error
 *  @retval DDCRC_ARG invalid display reference
 *
 *  @since 1.2.0
 */
DDCA_Status
ddca_get_display_info(
      DDCA_Display_Ref  ddca_dref,
      DDCA_Display_Info ** dinfo_loc);

/** @deprecated use #ddca_free_display_info2()
 *  Frees a #DDCA_Display_Info struct.
 *
 *  @param[in] info_rec pointer to instance to free
 *
 *  @remark
 *  This is a convenience function. #DDCA_Display_Info is copied to
 *  the client and contains no pointers.  It could simply be free()'d
 *  by the client.
 *
 *  @since 1.2.0
 */
void
ddca_free_display_info(DDCA_Display_Info * info_rec);


/** Gets publicly visible information about a display reference
 *
 *  The returned struct can simply be free()'d by the client.
 *
 *  @param[in]  ddca_dref display reference
 *  @param[out] dinfo_loc where to return pointer to newly allocated #DDCA_Display_Info
 *  @retval DDCRC_OK  no error
 *  @retval DDCRC_ARG invalid display reference
 *
 *  @since 2.2.1
 */
DDCA_Status
ddca_get_display_info2(
      DDCA_Display_Ref   ddca_dref,
      DDCA_Display_Info2 ** dinfo_loc);

/** Frees a #DDCA_Display_Info2 struct.
 *
 *  @param[in] info_rec pointer to instance to free
 *
 *  @remark
 *  This is a convenience function. #DDCA_Display_Info2 is copied to
 *  the client and contains no pointers.  It could simply be free()'d
 *  by the client.
 *
 *  @since 2.2.1
 */
void
ddca_free_display_info2(DDCA_Display_Info2 * info_rec);

/** Gets a list of the detected displays.
 *
 *  @param[in]  include_invalid_displays if true, displays that do not support DDC are included
 *  @param[out] dlist_loc where to return pointer to #DDCA_Display_Info_List
 *  @retval     0  always succeeds
 */
DDCA_Status
ddca_get_display_info_list2(
      bool                      include_invalid_displays,
      DDCA_Display_Info_List**  dlist_loc);

/** Frees a list of detected displays.
 *
 *  @param[in] dlist pointer to #DDCA_Display_Info_List
 *
 *  @remark
 *  This is a convenience function. #DDCA_Display_Info_List
 *  contains no pointers and is copied to the client. It could
 *  simply be free'd by the client.
 */
void
ddca_free_display_info_list(
      DDCA_Display_Info_List * dlist);

/** @deprecated use report_display_info2()
 *  Presents a report on a single display.
 *  The report is written to the current FOUT device for the current thread.
 *
 *  @param[in]  dinfo  pointer to a DDCA_Display_Info struct
 *  @param[in]  depth  logical indentation depth
 *  @retval     DDCRC_ARG if precondition failure and precondition failures do not abort
 *  @retval     0         normal
 *
 *  @remark
 *  For a report intended for users, apply #ddca_report_display_by_dref()
 *  to **dinfo->dref**.
 */
DDCA_Status
ddca_report_display_info(
      DDCA_Display_Info * dinfo,
      int                 depth);

/** Presents a report on a single display.
 *  The report is written to the current FOUT device for the current thread.
 *
 *  @param[in]  dinfo  pointer to a DDCA_Display_Info struct
 *  @param[in]  depth  logical indentation depth
 *  @retval     DDCRC_ARG if precondition failure and precondition failures do not abort
 *  @retval     0         normal
 *
 *  @remark
 *  For a report intended for users, apply #ddca_report_display_by_dref()
 *  to **dinfo->dref**.
 *
 *  @since 2.2.1
 */
DDCA_Status
ddca_report_display_info2(
      DDCA_Display_Info2 * dinfo,
      int                 depth);


/** Reports on all displays in a list of displays.
 *  The report is written to the current FOUT device for the current thread.
 *
 *  @param[in]  dlist  pointer to a DDCA_Display_Info_List
 *  @param[in]  depth  logical indentation depth
 *
 *  @remark
 *  This is a convenience function.
 */
void
ddca_report_display_info_list(
      DDCA_Display_Info_List * dlist,
      int                      depth);

/** Reports on all active displays.
 *  This function hooks into the code used by command "ddcutil detect"
 *
 *  @param[in] include_invalid_displays if true, report displays that don't support DDC
 *  @param[in] depth  logical indentation depth
 *  @return    number of MCCS capable displays
 */
int
ddca_report_displays(
      bool include_invalid_displays,
      int  depth);

/** Reinitializes detected displays
 *
 *  - closes all open displays, releasing any display locks
 *  - n. all existing display handles become invalid
 *  - releases display refs (all existing display refs become invalid)
 *  - releases i2c bus info
 *  - rescans i2c buses
 *  - redetects displays
 *
 *  @retval 0     normal execution
 *  @retval DDCRC_INVALID_OPERATION recursive call, or display watch unimplemented
 *  
 *  @since 1.2.0
 */
DDCA_Status
ddca_redetect_displays();


//
// Display Identifier
//

/** Creates a display identifier using the display number assigned by ddcutil
 *
 *  @param[in]  dispno  display number
 *  @param[out] did_loc    where to return display identifier handle
 *  @retval     0
 *  @ingroup api_display_spec
 */
DDCA_Status
ddca_create_dispno_display_identifier(
      int                      dispno,
      DDCA_Display_Identifier* did_loc);

/** Creates a display identifier using an I2C bus number
 *
 *  @param[in]  busno  I2C bus number
 *  @param[out] did_loc   where to return display identifier handle
 *  @retval     0
 *
 *  @ingroup api_display_spec
 */
DDCA_Status
ddca_create_busno_display_identifier(
      int                      busno,
      DDCA_Display_Identifier* did_loc);

/** Creates a display identifier using some combination of the manufacturer id,
 *  model name string and serial number string.  At least 1 of the 3 must be specified.
 *
 *  @param[in]  mfg_id    3 letter manufacturer id
 *  @param[in]  model     model name string
 *  @param[in]  sn        serial number string
 *  @param[out] did_loc   where to return display identifier handle
 *  @retval     0         success
 *  @retval     DDCRC_ARG all arguments NULL, or at least 1 too long
 *
 *  @ingroup api_display_spec
 */
DDCA_Status
ddca_create_mfg_model_sn_display_identifier(
      const char *             mfg_id,
      const char *             model,
      const char *             sn,
      DDCA_Display_Identifier* did_loc);

/** Creates a display identifier using a 128 byte EDID
 *
 *  @param[in]   edid       pointer to 128 byte EDID
 *  @param[out]  did_loc    where to return display identifier handle
 *  @retval      0          success
 *  @retval      DDCRC_ARG  edid==NULL
 *
 * @ingroup api_display_spec
 */
DDCA_Status
ddca_create_edid_display_identifier(
      const uint8_t*            edid,
      DDCA_Display_Identifier * did_loc);      // 128 byte edid

/** Creates a display identifier using a USB bus number and device number
 *
 *  @param[in]  bus    USB bus number
 *  @param[in]  device USB device number
 *  @param[out] did_loc   where to return display identifier handle
 *  @retval 0 success
 *
 *   @ingroup api_display_spec
 */
DDCA_Status
ddca_create_usb_display_identifier(
      int                      bus,
      int                      device,
      DDCA_Display_Identifier* did_loc);

/** Creates a display identifier using a /dev/usb/hiddev device number
 *
 *  @param[in] hiddev_devno hiddev device number
 *  @param[out] did_loc   where to return display identifier handle
 *  @retval 0  success
 *
 *  @ingroup api_display_spec
 */
DDCA_Status
ddca_create_usb_hiddev_display_identifier(
      int                      hiddev_devno,
      DDCA_Display_Identifier* did_loc);


/** Release the memory of a display identifier
 *
 *  @param[in] did  display identifier, may be NULL
 *  @retval 0          success
 *  @retval DDCRC_ARG  invalid display identifier
 *
 *  @remark
 *  Does nothing and returns 0 if **did** is NULL.
 */
DDCA_Status
ddca_free_display_identifier(
      DDCA_Display_Identifier did);

/** Returns a string representation of a display identifier.
 *
 *  The string is valid until the display identifier is freed.
 *
 *  @param[in]  did    display identifier
 *  @return     string representation of display identifier, NULL if invalid
 *
 *  @ingroup api_display_spec
 */
const char *
ddca_did_repr(
      DDCA_Display_Identifier did);


//
// Display Reference
//

/** @deprecated use #ddca_get_display_ref()
 *  Gets a display reference for a display identifier.
 *  Normally, this is a permanently allocated #DDCA_Display_Ref
 *  created by monitor detection and does not need to be freed.
 *  Use #ddca_free_display_ref() to safely free.
 *
 *  @param[in]  did      display identifier
 *  @param[out] dref_loc where to return display reference
 *  @retval     0                     success
 *  @retval     DDCRC_ARG             did is not a valid display identifier handle
 *  @retval     DDCRC_INVALID_DISPLAY display not found
 *
 *  @ingroup api_display_spec
 */
// __attribute__ ((deprecated ("use ddca_get_display_ref()")))
DDCA_Status
ddca_create_display_ref(
      DDCA_Display_Identifier did,
      DDCA_Display_Ref*       dref_loc);


/** Gets a display reference for a display identifier.
 *  This is a permanently allocated #DDCA_Display_Ref
 *  created by monitor detection and does not need to be freed.
 *
 *  @param[in]  did      display identifier
 *  @param[out] dref_loc where to return display reference
 *  @retval     0                     success
 *  @retval     DDCRC_ARG             did is not a valid display identifier handle
 *  @retval     DDCRC_INVALID_DISPLAY display not found
 *
 *  @since 0.9.5
 *  @ingroup api_display_spec
 */
DDCA_Status
ddca_get_display_ref(
      DDCA_Display_Identifier did,
      DDCA_Display_Ref*       dref_loc);

/** Checks whether a #DDCA_Display_Ref is valid
 *
 *  @param   dref                  display reference to validate
 *  @param   require_not_asleep    include check for DPMS sleep state
 *  @retval  DDCRC_OK
 *  @retval  DDCRC_ARG             dref == NULL or does not point to a valid DDCA_Display_Ref
 *  @retval  DDCRC_INTERNAL_ERROR  dref->drm_connector == NULL
 *  @retval  DDCRC_DISCONNECTED    monitor has been disconnected
 *  @retval  DDCRC_DPMS_ASLEEP     monitor in a DPMS sleep state
 *  @retval  DDCRC_INVALID_DISPLAY not found
 *
 *  @since 2.1.0
 */
DDCA_Status
ddca_validate_display_ref(
      DDCA_Display_Ref  dref,
      bool              require_not_asleep);

/** Returns a string representation of a display reference
 *
 *  The returned value is valid until the next call to this function on
 *  the current thread.
 *
 *  @param[in]   dref display reference
 *  @return      string representation of display reference, NULL if invalid
 */
const char *
ddca_dref_repr(
      DDCA_Display_Ref dref);

/** Writes a report on the specified display reference to the current FOUT device
 *
 *  @param[in] dref   display reference
 *  @param[in] depth  logical indentation depth
 *
 *  @ingroup api_display_spec
 */
void
ddca_dbgrpt_display_ref(
      DDCA_Display_Ref dref,
      int              depth);


//
// Display Handle
//

/** Open a display
 *
 *  @param[in]  ddca_dref    display reference for display to open
 *  @param[in]  wait         if true, wait if display locked by another thread
 *  @param[out] ddca_dh_loc  where to return display handle
 *  @return     status code
 *
 *  Fails if display is already opened by another thread.
 *  @ingroup api_display_spec
 */
DDCA_Status
ddca_open_display2(
      DDCA_Display_Ref      ddca_dref,
      bool                  wait,
      DDCA_Display_Handle * ddca_dh_loc);

/** Close an open display
 *
 *  @param[in]  ddca_dh   display handle, if NULL do nothing
 *  @retval     DDCRC_OK  close succeeded, or ddca_dh == NULL
 *  @retval     DDCRC_ARG invalid handle
 *  @return     -errno    from underlying OS close()
 *
 *  @ingroup api_display_spec
 */
DDCA_Status
ddca_close_display(
      DDCA_Display_Handle   ddca_dh);

/** Returns a string representation of a display handle.
 *  The string is valid until until the handle is closed.
 *
 * @param[in] ddca_dh  display handle
 * @return string  representation of display handle, NULL if
 *                 argument is NULL or not a display handle
 *
 *  @ingroup api_display_spec
 */
const char *
ddca_dh_repr(
      DDCA_Display_Handle   ddca_dh);

// CHANGE NAME?
/** Returns the display reference for display handle.
 *
 *  @param[in] ddca_dh   display handle
 *  @return #DDCA_Display_Ref of the handle,
 *          NULL if invalid display handle
 *
 *  @since 0.9.0
 */
DDCA_Display_Ref
ddca_display_ref_from_handle(
      DDCA_Display_Handle   ddca_dh);


//
// Monitor Capabilities
//

/** Retrieves the capabilities string for a monitor.
 *
 *  @param[in]  ddca_dh     display handle
 *  @param[out] caps_loc    address at which to return pointer to capabilities string.
 *  @return     status code
 *
 *  It is the responsibility of the caller to free the returned string.
 */
DDCA_Status
ddca_get_capabilities_string(
      DDCA_Display_Handle     ddca_dh,
      char**                  caps_loc);

/** Parse the capabilities string.
 *
 *  @param[in] capabilities_string      unparsed capabilities string
 *  @param[out] parsed_capabilities_loc address at which to return pointer to newly allocated
 *                                      #DDCA_Capabilities struct
 *  @return     status code
 *
 *  It is the responsibility of the caller to free the returned struct
 *  using ddca_free_parsed_capabilities().
 *
 *  This function currently parses the VCP feature codes and MCCS version.
 *  It could be extended to parse additional information such as cmds if necessary.
 */
DDCA_Status
ddca_parse_capabilities_string(
      char *                   capabilities_string,
      DDCA_Capabilities **     parsed_capabilities_loc);

/** Frees a DDCA_Capabilities struct
 *
 *  @param[in] parsed_capabilities  pointer to struct to free,
 *                                  does nothing if NULL.
 */
void
ddca_free_parsed_capabilities(
      DDCA_Capabilities *      parsed_capabilities);

/** Reports the contents of a DDCA_Capabilities struct.
 *
 *  The report is written to the current FOUT location.
 *
 *  If the current output level is #DDCA_OL_VERBOSE, additional
 *  information is written, including command codes.
 *
 *  @param[in]  parsed_capabilities  pointer to #DDCA_Capabilities struct
 *  @param[in]  ddca_dref            display reference, may be NULL
 *  @param[in]  depth  logical       indentation depth
 *
 *  @remark
 *  If ddca_dref is not NULL, feature value names will reflect any loaded monitor definition files
 *  @since 0.9.3
 */
DDCA_Status
ddca_report_parsed_capabilities_by_dref(
      DDCA_Capabilities *      parsed_capabilities,
      DDCA_Display_Ref         ddca_dref,
      int                      depth);

/** Reports the contents of a DDCA_Capabilities struct.
 *
 *  The report is written to the current FOUT location.
 *
 *  If the current output level is #DDCA_OL_VERBOSE, additional
 *  information is written, including command codes.
 *
 *  @param[in]  parsed_capabilities  pointer to #DDCA_Capabilities struct
 *  @param[in]  ddca_dh              display handle, may be NULL
 *  @param[in]  depth                logical indentation depth
 *  @retval     0                    success
 *  @retval     DDCRC_ARG            invalid display handle
 *
 *  @remark
 *  If ddca_dh is not NULL, feature value names will reflect any loaded monitor definition files
 *  @since 0.9.3
 */
DDCA_Status
ddca_report_parsed_capabilities_by_dh(
      DDCA_Capabilities *      p_caps,
      DDCA_Display_Handle      ddca_dh,
      int                      depth);

/** Reports the contents of a DDCA_Capabilities struct.
 *
 *  The report is written to the current FOUT location.
 *
 *  If the current output level is #DDCA_OL_VERBOSE, additional
 *  information is written, including command codes.
 *
 *  @param[in]  parsed_capabilities  pointer to #DDCA_Capabilities struct
 *  @param[in]  ddca_dref            display reference
 *  @param[in]  depth  logical       indentation depth
 *
 *  @remark
 *  Any user supplied feature definitions for the monitor are ignored.
 */
void
ddca_report_parsed_capabilities(
      DDCA_Capabilities *      parsed_capabilities,
      int                      depth);

/** Returns the VCP feature codes defined in a
 *  parsed capabilities record as a #DDCA_Feature_List
 *
 *  @param[in] parsed_caps  parsed capabilities
 *  @return bitfield of feature ids
 *  @since 0.9.0
 */
DDCA_Feature_List
ddca_feature_list_from_capabilities(
      DDCA_Capabilities * parsed_caps);


//
//  MCCS Version Specification
//

/** Gets the MCCS version of a monitor.
 *
 *  @param[in]    ddca_dh   display handle
 *  @param[out]   p_vspec   where to return version spec
 *  @return       DDCRC_ARG invalid display handle
 *
 *  @remark Returns version 0.0 (#DDCA_VSPEC_UNKNOWN) if feature DF cannot be read
 */
DDCA_Status
ddca_get_mccs_version_by_dh(
      DDCA_Display_Handle     ddca_dh,
      DDCA_MCCS_Version_Spec* p_vspec);


//
// VCP Feature Metadata
//

/** Controls whether user defined features (aka dynamic features) are supported.
 *
 *  @param[in] onoff true/false
 *  @return    prior value
 *
 *  @since 0.9.3
 */
bool
ddca_enable_udf(bool onoff);

/** Query whether user defined features (aka dynamic features) are supported.
 *
 *  @retval true  UDF enabled
 *  @retval false UDF disabled
 *
 *  @since 0.9.3
 */
bool
ddca_is_udf_enabled(void);

/** Loads any user supplied feature definition files for the specified
 *  display.  Does nothing if they have already been loaded.
 *
 *  @param[in] ddca_dref display reference
 *  @return    status code
 *
 *  It is not a error if no feature definition file is found,
 *  Feature definition file errors can be retrieved using #ddca_get_error_detail().
 *
 *  @remark
 *  Loading feature definition files is a separate operation because errors
 *  are possible when reading and processing the definitions.
 *  @since 0.9.3
 */
DDCA_Status
ddca_dfr_check_by_dref(DDCA_Display_Ref ddca_dref);

/** Loads any user supplied feature definition files for the specified
 *  display.  Does nothing if they have already been loaded.
 *
 *  @param[in] ddca_dh display handle
 *  @return    status code
 *
 *  See #ddca_dfr_check_by_dref() for detailed documentation.
 *
 *  @since 0.9.3
 */
DDCA_Status
ddca_dfr_check_by_dh(DDCA_Display_Handle ddca_dh);

/** Gets metadata for a VCP feature.
 *
 *  @param[in]  vspec            VCP version
 *  @param[in]  feature_code     VCP feature code
 *  @param[in]  create_default_if_not_found
 *  @param[out] meta_loc         return pointer to metadata here
 *  @return     status code
 *  @retval     DDCRC_ARG        invalid display handle
 *  @retval     DDCRC_UNKNOWN_FEATURE unrecognized feature code and
 *                              !create_default_if_not_found
 *
 *  It is the responsibility of the caller to free the returned DDCA_Feature_Metadata instance.
 *
 *  @remark
 *  Note that VCP characteristics (C vs NC, RW vs RO, etc) can vary by MCCS version.
 *  @remark
 *  Only takes into account VCP version.  Useful for reporting display agnostic
 *  feature information.  For display sensitive feature information, i.e. taking
 *  into account the specific monitor model, use #ddca_get_feature_metdata_by_dref()
 *  or #ddca_get_feature_metadata_by_dh().
 *
 * @since 0.9.3
 */
DDCA_Status
ddca_get_feature_metadata_by_vspec(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_MCCS_Version_Spec      vspec,
      bool                        create_default_if_not_found,
      DDCA_Feature_Metadata **    meta_loc);

/** Gets metadata for a VCP feature.
 *
 *  Note that VCP characteristics (C vs NC, RW vs RO, etc) can vary by MCCS version.
 *
 *  @param[in]  ddca_dref        display reference
 *  @param[in]  feature_code     VCP feature code
 *  @param[in]  create_default_if_not_found
 *  @param[out] meta_loc         return pointer to metadata here
 *  @return     status code
 *  @retval     DDCRC_ARG        invalid display reference
 *  @retval     DDCRC_UNKNOWN_FEATURE unrecognized feature code and
 *                              !create_default_if_not_found
 *
 *  It is the responsibility of the caller to free the returned DDCA_Feature_Metadata instance.
 *
 *  @remark
 *  This function first checks if there is a user supplied feature definition
 *  for the monitor.  If not, it looks up feature metadata based on the
 *  VCP version of the monitor.
 *  @remark
 *  Note that feature characteristics (C vs NC, RW vs RO, etc) can vary by MCCS version.
 *  @since 0.9.3
 */
DDCA_Status
ddca_get_feature_metadata_by_dref(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Display_Ref            ddca_dref,
      bool                        create_default_if_not_found,
      DDCA_Feature_Metadata **    meta_loc);

/** Gets metadata for a VCP feature.
 *
 *  @param[in]  ddca_dh          display handle
 *  @param[in]  feature_code     VCP feature code
 *  @param[in]  create_default_if_not_found
 *  @param[out] meta_loc         return pointer to metadata here
 *  @return     status code
 *  @retval     DDCRC_ARG        invalid display handle
 *  @retval     DDCRC_UNKNOWN_FEATURE unrecognized feature code and
 *                              !create_default_if_not_found
 *
 *  It is the responsibility of the caller to free the returned DDCA_Feature_Metadata instance.
 *
 *  @remark
 *  This function first checks if there is a user supplied feature definition
 *  for the monitor.  If not, it looks up feature metadata based on the
 *  VCP version of the monitor.
 *  @remark
 *  Note that feature characteristics (C vs NC, RW vs RO, etc) can vary by MCCS version.
 *  @since 0.9.3
 */
DDCA_Status
ddca_get_feature_metadata_by_dh(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Display_Handle         ddca_dh,
      bool                        create_default_if_not_found,
      DDCA_Feature_Metadata **    meta_loc);

/** Frees a #DDCA_Feature_Metadata instance
 *
 *  @param[in] metadata pointer to instance
 *  @retval   0  normal
 *  @since 0.9.3
 *
 *  @remark
 *  It is not an error if the ***metadata*** pointer argument is NULL
 *  @remark
 *  This is a convenience function.
 */
void
ddca_free_feature_metadata(DDCA_Feature_Metadata * metadata);

/** Gets the VCP feature name.  If different MCCS versions use different names
 *  for the feature, this function makes a best guess.
 *
 *  @param[in]  feature_code feature code
 *  @return     pointer to feature name (do not free), NULL if unknown feature code
 *
 *  @remark
 *  Since no specific display is indicated, this function ignores user defined
 *  monitor feature information.
 */
const char *
ddca_get_feature_name(DDCA_Vcp_Feature_Code feature_code);


/** Convenience function that searches a Feature Value Table for a
 *  value and returns the corresponding name.
 *
 *  @param[in]   feature_value_table  pointer to first entry of table
 *  @param[in]   feature_value        value to search for
 *  @param[out]  value_name_loc       where to return pointer to name
 *  @retval      DDCRC_OK  value found
 *  @retval      DDCRC_NOT_FOUND  value not found
 *
 *  @remark
 *  The value returned in **value_name_loc** is a pointer into the table
 *  data structure.  Do not free.
 *
 *  @remark
 *  This is a convenience function.
 */
DDCA_Status
ddca_get_simple_nc_feature_value_name_by_table(
      DDCA_Feature_Value_Entry *  feature_value_table,
      uint8_t                     feature_value,
      char**                      value_name_loc);

/** Outputs a debugging report of the @DDCA_Feature_Metadata data structure.
 *
 *  @param[in] md    pointer to @DDCA_Feature_Metadata instance
 *  @param[in] depth logical indentation depth
 */
void
ddca_dbgrpt_feature_metadata(
      DDCA_Feature_Metadata * md,
      int                     depth);


//
//  Miscellaneous Monitor Specific Functions
//

/** Shows information about a display, specified by a #Display_Ref
 *
 *  Output is written using report functions
 *
 *  @param[in] dref       pointer to display reference
 *  @param[in] depth      logical indentation depth
 *  @retval DDCRC_ARG invalid display ref
 *  @retval 0         success
 *
 *  @remark
 *  The detail level shown is controlled by the output level setting
 *  for the current thread.
 *
 *  @since 0.9.0
 */
DDCA_Status
ddca_report_display_by_dref(DDCA_Display_Ref dref, int depth);

//
// Feature Lists
//
// Specifies a collection of VCP features as a 256 bit array of flags.
//

/** Empty feature list
 *  @since 0.9.0
 */
extern const DDCA_Feature_List DDCA_EMPTY_FEATURE_LIST;

/** Returns feature list symbolic name (for debug messages)
 *
 *  @param[in] feature_set_id
 *  @return symbolic name (do not free)
 *
 * @remark
 * This is a convenience function.
 */
const char *
ddca_feature_list_id_name(
      DDCA_Feature_Subset_Id  feature_set_id);

/** Given a feature set id, returns a #DDCA_Feature_List specifying all the
 *  feature codes in the set.
 *
 *  @param[in]  feature_set_id
 *  @param[in]  dref                   display reference
 *  @param[in]  include_table_features if true, Table type features are included
 *  @param[out] points to feature list to be filled in
 *  @retval     DDCRC_ARG  invalid display reference
 *  @retval     DDCRC_OK   success
 *
 *  @since 0.9.0
 */
DDCA_Status
ddca_get_feature_list_by_dref(
      DDCA_Feature_Subset_Id  feature_set_id,
      DDCA_Display_Ref        dref,
      bool                    include_table_features,
      DDCA_Feature_List*      feature_list_loc);

/** Empties a #DDCA_Feature_List
 *
 *  @param[in]  vcplist pointer to feature list
 *
 *  @remark
 *  Alternatively, just set vcplist = DDCA_EMPTY_FEATURE_LIST
 *  @remark
 *  This is a convenience function.
 *  @since 0.9.0
 */
void
ddca_feature_list_clear(
      DDCA_Feature_List* vcplist);


/** Adds a feature code to a #DDCA_Feature_List
 *
 *  @param[in]  vcplist   pointer to feature list
 *  @param[in]  vcp_code  VCP feature code
 *  @return     modified feature list
 *
 *  @remark
 *  The feature list is modified in place and also returned.
 *
 *  @since 0.9.0
 */
DDCA_Feature_List
ddca_feature_list_add(
      DDCA_Feature_List* vcplist,
      uint8_t vcp_code);

/** Tests if a #DDCA_Feature_List contains a VCP feature code
 *
 *  @param[in]  vcplist   feature list
 *  @param[in]  vcp_code  VCP feature code
 *  @return     true/false
 *
 *  @since 0.9.0
 */
bool
ddca_feature_list_contains(
      DDCA_Feature_List vcplist,
      uint8_t vcp_code);

/** Tests if 2 feature lists are equal.
 *
 *  @param[in] vcplist1   first feature list
 *  @param[in] vcplist2   second feature list
 *  @return true if they contain the same features, false if not
 *
 *  @remark
 *  The input feature lists are not modified.
 *  @since 0.9.9
 */
bool
ddca_feature_list_eq(
      DDCA_Feature_List vcplist1,
      DDCA_Feature_List vcplist2);

/** Creates a union of 2 feature lists.
 *
 *  @param[in] vcplist1   first feature list
 *  @param[in] vcplist2   second feature list
 *  @return feature list in which a feature is set if it is in either
 *          of the 2 input feature lists
 *
 *  @remark
 *  The input feature lists are not modified.
 *  @since 0.9.0
 */
DDCA_Feature_List
ddca_feature_list_or(
      DDCA_Feature_List vcplist1,
      DDCA_Feature_List vcplist2);

/** Creates the intersection of 2 feature lists.
 *
 *  @param[in] vcplist1   first feature list
 *  @param[in] vcplist2   second feature list
 *  @return feature list in which a feature is set if it is in both
 *          of the 2 input feature lists
 *
 *  @remark
 *  The input feature lists are not modified.
 *  @since 0.9.0
 */
DDCA_Feature_List
ddca_feature_list_and(
      DDCA_Feature_List vcplist1,
      DDCA_Feature_List vcplist2);

/** Returns a feature list consisting of all the features in the
 *  first list that are not in the second.
 *
 *  @param[in] vcplist1   first feature list
 *  @param[in] vcplist2   second feature list
 *  @return feature list in which a feature is set if it is in **vcplist1** but
 *          not **vcplist2**
 *
 *  @remark
 *  The input feature lists are not modified.
 *  @since 0.9.0
 */
DDCA_Feature_List
ddca_feature_list_and_not(
      DDCA_Feature_List vcplist1,
      DDCA_Feature_List vcplist2);

/** Returns the number of features in a feature list
 *
 *  @param[in] feature_list   feature list
 *  @return  number of features, 0 if feature_list == NULL
 *
 *  @since 0.9.0
 */
int
ddca_feature_list_count(
      DDCA_Feature_List feature_list);

/** Returns a string representation of a feature list as a
 *  sequence of 2 character hex values.
 *
 *  @param[in] feature_list   feature list
 *  @param[in] value_prefix   precede each value with this string, e.g. "0x"
 *                            if NULL, then no preceding string
 *  @param[in] sepstr         separator string between pair of values, e.g. ", "
 *                            if NULL, then no separator string
 *  @return    string representation; The value is valid until the next call
 *             to this function in the current thread.  Caller should not free.
 *
 *  @since 0.9.0
 */
const char *
ddca_feature_list_string(
      DDCA_Feature_List feature_list,
      const char * value_prefix,
      const char * sepstr);


//
// GET AND SET VCP VALUES
//

/*
 * The API for getting and setting VCP values is doubly specified,
 * with both functions specific to Non-Table and Table values,
 * and more generic functions that can handle values of any type.
 *
 * As a practical matter, Table type features have not been observed
 * on any monitors (as of 3/2018), and applications can probably
 * safely be implemented using only the Non-Table APIs.
 *
 * Note that the functions for #DDCA_Any_Vcp_Value replace those
 * that previously existed for #DDCA_Single_Vcp_Value.
 */

//
// Free VCP Feature Value
//
// Note there is no function to free a #Non_Table_Vcp_Value, since
// this is a fixed size struct always allocated by the caller.
//

/** Frees a #DDCA_Table_Vcp_Value instance.
 *
 *  @param[in] table_value
 *
 *  @remark
 *  Was previously named **ddca_free_table_value_response().
 *  @since 0.9.0
 */
void
ddca_free_table_vcp_value(
      DDCA_Table_Vcp_Value * table_value);

/** Frees a #DDCA_Any_Vcp_Value instance.
 *
 *  @param[in] valrec  pointer to #DDCA_Any_Vcp_Value instance
 *  @since 0.9.0
 */
void
ddca_free_any_vcp_value(
      DDCA_Any_Vcp_Value * valrec);


//
// Get VCP Feature Value
//

/** Gets the value of a non-table VCP feature.
 *
 *  @param[in]  ddca_dh       display handle
 *  @param[in]  feature_code  VCP feature code
 *  @param[out] valrec        pointer to response buffer provided by the caller,
 *                           which will be filled in
 *  @return status code
 *
 *  @remark
 *  If the returned status code is other than **DDCRC_OK**, a detailed
 *  error report can be obtained using #ddca_get_error_detail()
 *  @remark
 *  Renamed from **ddca_get_nontable_vcp_value()**
 *  @since 0.9.0
 */
DDCA_Status
ddca_get_non_table_vcp_value(
       DDCA_Display_Handle        ddca_dh,
       DDCA_Vcp_Feature_Code      feature_code,
       DDCA_Non_Table_Vcp_Value*  valrec);

/** Gets the value of a table VCP feature.
 *
 *  @param[in]  ddca_dh         display handle
 *  @param[in]  feature_code    VCP feature code
 *  @param[out] table_value_loc address at which to return the value
 *  @return status code
 *
 *  @remark
 *  If the returned status code is other than **DDCRC_OK**, a detailed
 *  error report can be obtained using #ddca_get_error_detail()
 *  @note
 *  Implemented, but untested
 */
DDCA_Status
ddca_get_table_vcp_value(
       DDCA_Display_Handle     ddca_dh,
       DDCA_Vcp_Feature_Code   feature_code,
       DDCA_Table_Vcp_Value ** table_value_loc);

/** Gets the value of a VCP feature of any type.
 *
 *  @param[in]  ddca_dh       display handle
 *  @param[in]  feature_code  VCP feature code
 *  @param[in]  value_type    value type
 *  @param[out] valrec_loc    address at which to return a pointer to a newly
 *                           allocated #DDCA_Any_Vcp_Value
 *  @return status code
 *
 *  @remark
 *  If the returned status code is other than **DDCRC_OK**, a detailed
 *  error report can be obtained using #ddca_get_error_detail()
 *  @remark
 *  Replaces **ddca_get_any_vcp_value()
 *
 *  @since 0.9.0
 */
DDCA_Status
ddca_get_any_vcp_value_using_explicit_type(
       DDCA_Display_Handle         ddca_dh,
       DDCA_Vcp_Feature_Code       feature_code,
       DDCA_Vcp_Value_Type         value_type,
       DDCA_Any_Vcp_Value **       valrec_loc);

/** Gets the value of a VCP feature of any type.
 *  The type is determined by using ddcutil's internal
 *  feature description table.
 *
 *  Note that this function cannot be used for manufacturer-specific
 *  feature codes (i.e. those in the range xE0..xFF), since ddcutil
 *  does not know their type information.  Nor can it be used for
 *  unrecognized feature codes.
 *
 *  @param[in]  ddca_dh       display handle
 *  @param[in]  feature_code  VCP feature code
 *  @param[out] valrec_loc    address at which to return a pointer to a newly
 *                           allocated #DDCA_Any_Vcp_Value
 *  @return status code
 *
 *  @remark
 *  It an error to call this function for a manufacturer-specific feature or
 *  an unrecognized feature.
 *  @remark
 *  If the returned status code is other than **DDCRC_OK**, a detailed
 *  error report can be obtained using #ddca_get_error_detail()
 */
DDCA_Status
ddca_get_any_vcp_value_using_implicit_type(
       DDCA_Display_Handle         ddca_dh,
       DDCA_Vcp_Feature_Code       feature_code,
       DDCA_Any_Vcp_Value **       valrec_loc);


/** Returns a formatted representation of a table VCP value.
 *  It is the responsibility of the caller to free the returned string.
 *
 *  @param[in]  feature_code        VCP feature code
 *  @param[in]  dref                display reference
 *  @param[in]  table_value         table VCP value
 *  @param[out] formatted_value_loc address at which to return the formatted value.
 *  @return                         status code, 0 if success
 *  @since 0.9.0
 */
DDCA_Status
ddca_format_table_vcp_value_by_dref(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Display_Ref        ddca_dref,
      DDCA_Table_Vcp_Value *  table_value,
      char **                 formatted_value_loc);

/** Returns a formatted representation of a non-table VCP value.
 *  It is the responsibility of the caller to free the returned string.
 *
 *  @param[in]  feature_code        VCP feature code
 *  @param[in]  dref                display reference
 *  @param[in]  valrec              non-table VCP value
 *  @param[out] formatted_value_loc address at which to return the formatted value.
 *  @return                         status code, 0 if success
 *  @since 0.9.0
 */
DDCA_Status
ddca_format_non_table_vcp_value_by_dref(
      DDCA_Vcp_Feature_Code       feature_code,
      DDCA_Display_Ref            dref,
      DDCA_Non_Table_Vcp_Value *  valrec,
      char **                     formatted_value_loc);

/** Returns a formatted representation of a VCP value of any type
 *  It is the responsibility of the caller to free the returned string.
 *
 *  @param[in]  feature_code        VCP feature code
 *  @param[in]  dref                display reference
 *  @param[in]  valrec              non-table VCP value
 *  @param[out] formatted_value_loc address at which to return the formatted value.
 *  @return                         status code, 0 if success
 *  @since 0.9.0
 */
DDCA_Status
ddca_format_any_vcp_value_by_dref(
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Display_Ref        dref,
      DDCA_Any_Vcp_Value *    valrec,
      char **                 formatted_value_loc);


//
// Set VCP value
//

/** Sets a non-table VCP value by specifying it's high and low bytes individually.
 *
 *  @param[in]   ddca_dh             display handle
 *  @param[in]   feature_code        feature code
 *  @param[in]   hi_byte             high byte of new value
 *  @param[in]   lo_byte             low byte of new value
 *  @return      status code
 */
DDCA_Status
ddca_set_non_table_vcp_value(
      DDCA_Display_Handle      ddca_dh,
      DDCA_Vcp_Feature_Code    feature_code,
      uint8_t                  hi_byte,
      uint8_t                  lo_byte
     );

/** Sets a Table VCP value.
 *
 *  @param[in]   ddca_dh             display handle
 *  @param[in]   feature_code        feature code
 *  @param[in]   new_value           value to set
 *  @return      status code
 *  @since 0.9.0
 */
DDCA_Status
ddca_set_table_vcp_value(
      DDCA_Display_Handle      ddca_dh,
      DDCA_Vcp_Feature_Code    feature_code,
      DDCA_Table_Vcp_Value *   new_value);

/** Sets a VCP value of any type.
 *
 *  @param[in]   ddca_dh        display handle
 *  @param[in]   feature_code   feature code
 *  @param[in]   new_value      value to set
 *  @return      status code
 *  @since 0.9.0
 */
DDCA_Status
ddca_set_any_vcp_value(
      DDCA_Display_Handle     ddca_dh,
      DDCA_Vcp_Feature_Code   feature_code,
      DDCA_Any_Vcp_Value *    new_value);


//
// Get or set multiple values
//
// These functions provide an API version of the **dumpvcp** and **loadvcp**
// commands.
//

/** Returns a string containing monitor identification and values
 *  for all detected features that should be saved when a monitor is
 *  calibrated and restored when the calibration is applied.
 *
 *  @param[in]  ddca_dh                   display handle
 *  @param[out] profile_values_string_loc address at which to return string,
 *                                        caller is responsible for freeing
 *  @return     status code
 */
DDCA_Status
ddca_get_profile_related_values(
      DDCA_Display_Handle  ddca_dh,
      char**               profile_values_string_loc);

/** Sets multiple feature values for a specified monitor.
 *  The monitor identification and feature values are
 *  encoded in the string.
 *
 *  @param[in] ddca_dh display handle
 *  @param[in] profile_values_string string containing values
 *  @return     status code
 *
 *  @remark
 *  If **ddca_dh** is NULL, this function opens the first display
 *  that matches the display identifiers in the **profile_values_string**.
 *  If **ddca_dh** is non-NULL, then the identifiers in
 *  **profile_values_string**  must be consistent with the open display.
 *  @remark
 *  The non-NULL case exists to handle the unusual situation where multiple
 *  displays have the same manufacturer, model, and serial number,
 *  perhaps because the EDID has been cloned.
 *  @remark
 *  If the returned status code is **DDCRC_BAD_DATA** (others?), a detailed
 *  error report can be obtained using #ddca_get_error_detail()
 */
DDCA_Status
ddca_set_profile_related_values(
      DDCA_Display_Handle  ddca_dh,
      char *               profile_values_string);


//
//  Report display status changes
//

/** Registers a function to be called called when a change in display status is
 *  detected. It is not an error if the function is already registered.
 *
 *  @param[in] func   function of type #DDCA_Display_Detection_Callback_Func()
 *  @return    DDCRC_OK
 *  @retval    DDCRC_INVALID_OPERATION ddcutil not built with UDEV support,
 *                                     or not all video devices support DRM
 *
 *  @since 2.1.0
 */
DDCA_Status
ddca_register_display_status_callback(DDCA_Display_Status_Callback_Func func);

/** Removes a function from the list of registered callbacks
 *
 *  @param[in] func            function that has already been registered
 *  @retval    DDCRC_OK        function removed from list
 *  @retval    DDCRC_INVALID_OPERATION ddcutil not built with UDEV support,
 *                                     or not all video devices support DRM
 *  @retval    DDCRC_NOT_FOUND function not registered
 *
 *  @since 2.1.0
 */
DDCA_Status
ddca_unregister_display_status_callback(DDCA_Display_Status_Callback_Func func);

/** Returns the name of a #DDCA_Display_Event_Class
 *
 *  @param  event_class event class id
 *  @return             printable name
 *
 *  @remark
 *  The value returned exists in an internal ddcutil table.
 *  Caller should not free.
 *
 *  @since 2.1.0
 */
const char *
   ddca_display_event_class_name(DDCA_Display_Event_Class event_class);

/** Returns the name of a #DDCA_Display_Event_Type
 *
 *  @param  event_type  event type id
 *  @return             printable event type name
 *
 *  @remark
 *  The value returned exists in an internal ddcutil table.
 *  Caller should not free.
 *
 *  @since 2.1.0
 */
const char *
   ddca_display_event_type_name(DDCA_Display_Event_Type event_type);


/** Start the threads watching for display status changes.
 *
 *  @param  enabled_clases  event classes to watch
 *  @retval DDCRC_OK
 *  #retval DDCRC_ARG     no event classes or invalid event classes specified
 *  @retval DDCRC_INVALID_OPERATION watch thread already running
 *  @retval DDCRC_INVALID_OPERATION not all video drivers support DRM
 *  @retval DDCRC_UNIMPLEMENTED watching for DPMS changes unimplemented
 *
 *  @since 2.1.0
 *
 *  @remark
 *  As of ddcutil 2.2.0, the only valid event_type value is
 *  DDCA_EVENT_CLASS_DISPLAY_CONNECTION. DDCA_EVENT_CLASS_ALL
 *  is equivalent to DDCA_EVENT_CLASS_DISPLAY_CONNECTION.
 */
DDCA_Status
ddca_start_watch_displays(DDCA_Display_Event_Class enabled_classes);

/** Terminate the threads that watch for display status changes.
 *
 *  This function is a hack.  Without it, the thread can
 *  continue running even though the application has exited.
 *
 *  @param wait  Wait for watch thread to actually terminate
 *  @retval DDCRC_OK
 *  @retval DDCRC_INVALID_OPERATION  watch thread not executing
 *
 *  If this function is being called as part of termination
 *  by the client, there's no need to wait for the watch thread
 *  to actually finish.
 *
 *  @since 2.1.0
 */
DDCA_Status
ddca_stop_watch_displays(bool wait);

/** If the watch thread is currently executing returns, reports the
 *  currently active display event classes as a bit flag.
 *
 *  @param  classes_loc  where to return bit flag
 *  @retval DDCRC_OK
 *  @retval DDCRC_INVALID_OPERATION watch thread not executing
 */
DDCA_Status
ddca_get_active_watch_classes(DDCA_Display_Event_Class * classes_loc);

/** Retrieve current display watch settings into a buffer provided
 *  by the caller.
 *
 *  @param settings_buffer pointer to caller buffer
 *  @retval DDCRC_OK
 *  @retval DDCRC_UNINITIALIZED
 *
 *  @since 2.2.0
 */
DDCA_Status
ddca_get_display_watch_settings(DDCA_DW_Settings * settings_buffer);

/** Modify the current display watch settings.
 *
 *  @param settings_buffer pointer to settings buffer
 *  @retval DDCRC_OK
 *  @retval DDCRC_ARG
 *  @retval DDCRC_UNINITIALIZED
 *
 *  @since 2.2.0
 */
DDCA_Status
ddca_set_display_watch_settings(DDCA_DW_Settings * settings_buffer);


#ifdef __cplusplus
}
#endif
#endif /* DDCUTIL_C_API_H_ */
