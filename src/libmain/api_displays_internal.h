/** @file api_displays_internal.h
 *
 *  For use only by other api_... files.
 */

// Copyright (C) 2015-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later
 

#ifndef API_DISPLAYS_INTERNAL_H_
#define API_DISPLAYS_INTERNAL_H_

#include "public/ddcutil_types.h"
#include "private/ddcutil_types_private.h"


Display_Ref * validated_ddca_display_ref(DDCA_Display_Ref ddca_dref);
Display_Handle * validated_ddca_display_handle(DDCA_Display_Handle ddca_dh);

#define VALIDATE_DDCA_DREF(_ddca_dref, _dref, _debug) \
   do { \
      _dref = validated_ddca_display_ref(_ddca_dref); \
      if (!_dref) { \
         DBGTRC(_debug, DDCA_TRC_API, "Done.     Returning DDCRC_ARG"); \
         return DDCRC_ARG; \
      } \
   } while(0)

#define WITH_VALIDATED_DR(ddca_dref, action) \
   do { \
      assert(library_initialized); \
      DDCA_Status psc = 0; \
      free_thread_error_detail(); \
      Display_Ref * dref = validated_ddca_display_ref(ddca_dref); \
      if (!dref)  { \
         psc = DDCRC_ARG; \
      } \
      else { \
         (action); \
      } \
      return psc; \
   } while(0);

#define WITH_VALIDATED_DR3(_ddca_dref, _ddcrc, _action) \
   do { \
      assert(library_initialized); \
      _ddcrc = 0; \
      free_thread_error_detail(); \
      Display_Ref * dref = validated_ddca_display_ref(_ddca_dref); \
      if (!dref)  { \
         _ddcrc = DDCRC_ARG; \
      } \
      else { \
         (_action); \
      } \
   } while(0);


#define WITH_VALIDATED_DH2(ddca_dh, action) \
   do { \
      assert(library_initialized); \
      DDCA_Status psc = 0; \
      free_thread_error_detail(); \
      Display_Handle * dh = validated_ddca_display_handle(ddca_dh); \
      if (!dh)  { \
         psc = DDCRC_ARG; \
         DBGTRC_DONE(debug, DDCA_TRC_API, "Returning: %s. ddca_dh=%p", psc_name_code(psc), ddca_dh); \
      } \
      else { \
         (action); \
      } \
      return psc; \
   } while(0);


// extern DDCA_Monitor_Model_Key DDCA_UNDEFINED_MONITOR_MODEL_KEY;

// Monitor Model Key - UNPUBLISHED, USED INTERNALLY

//
// Monitor Model Identifier
//

/** Special reserved value indicating value undefined.
 * @since 0.9.0
 */
const extern DDCA_Monitor_Model_Key DDCA_UNDEFINED_MONITOR_MODEL_KEY;

/** Creates a monitor model identifier.
 *
 *  @param  mfg_id
 *  @param  model_name
 *  @param  product_code
 *  @return identifier (note the value returned is the actual identifier,
 *                     not a pointer)
 *  @retval DDCA_UNDEFINED_MONITOR_MODEL_KEY if parms are invalid
 *  @since 0.9.0
 */
DDCA_Monitor_Model_Key
ddca_mmk(
      const char * mfg_id,
      const char * model_name,
      uint16_t     product_code);

/** Tests if 2 #Monitor_Model_Key identifiers specify the
 *  same monitor model.
 *
 *  @param  mmk1   first identifier
 *  @param  mmk2   second identifier
 *
 *  @remark
 *  The identifiers are considered equal if both are defined.
 *  @since 0.9.0
 */

bool
ddca_mmk_eq(
      DDCA_Monitor_Model_Key mmk1,
      DDCA_Monitor_Model_Key mmk2);

/** Tests if a #Monitor_Model_Key value
 *  represents a defined identifier.
 *
 *  @param mmk
 *  @return true/false
 *  @since 0.9.0
 */
bool
ddca_mmk_is_defined(
      DDCA_Monitor_Model_Key mmk);


/** Extracts the monitor model identifier for a display represented by
 *  a #DDCA_Display_Ref.
 *
 *  @param ddca_dref
 *  @return monitor model identifier
 *  @since 0.9.0
 */
DDCA_Monitor_Model_Key
ddca_mmk_from_dref(
      DDCA_Display_Ref   ddca_dref);


// CHANGE NAME?  _for_dh()?   ddca_mmid_for_dh()
/** Returns the monitor model identifier for an open display.
 *
 *  @param  ddca_dh   display handle
 *  @return #DDCA_Monitor_Model_Key for the handle,
 *          NULL if invalid display handle
 *
 *  @since 0.9.0
 */
DDCA_Monitor_Model_Key
ddca_mmk_from_dh(
      DDCA_Display_Handle   ddca_dh);


// DEPRECATED IN  0.9.0

/** @deprecated use #ddca_get_display_info_list2()
 * Gets a list of the detected displays.
 *
 *  Displays that do not support DDC are not included.
 *
 *  @return list of display summaries
 */
__attribute__ ((deprecated ("use ddca_get_display_info_list2()")))
DDCA_Display_Info_List *
ddca_get_display_info_list(void);


/** \deprecated use #ddca_report_displays()
 * Reports on all active displays.
 *  This function hooks into the code used by command "ddcutil detect"
 *
 *  @param[in] depth  logical indentation depth
 *  @return    number of MCCS capable displays
 */
__attribute__ ((deprecated ("use ddca_report_displays()")))
int
ddca_report_active_displays(
      int depth);


// /** \deprecated */
__attribute__ ((deprecated))
DDCA_Status
ddca_get_edid_by_dref(
      DDCA_Display_Ref ddca_dref,
      uint8_t **       pbytes_loc);   // pointer into ddcutil data structures, do not free


/** \deprecated Use #ddca_open_display2()
 * Open a display
 * @param[in]  ddca_dref    display reference for display to open
 * @param[out] ddca_dh_loc  where to return display handle
 * @return     status code
 *
 * Fails if display is already opened by another thread.
 * \ingroup api_display_spec
 */
// __attribute__ ((deprecated ("use ddca_open_display2()")))
DDCA_Status
ddca_open_display(
      DDCA_Display_Ref      ddca_dref,
      DDCA_Display_Handle * ddca_dh_loc);



#endif /* API_DISPLAYS_INTERNAL_H_ */
