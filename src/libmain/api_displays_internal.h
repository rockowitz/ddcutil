/** @file api_displays_internal.h
 *
 *  For use only by other api_... files.
 */

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later
 

#ifndef API_DISPLAYS_INTERNAL_H_
#define API_DISPLAYS_INTERNAL_H_

#include "public/ddcutil_types.h"
#include "private/ddcutil_types_private.h"


#define WITH_DR(ddca_dref, action) \
   do { \
      if (!library_initialized) \
         return DDCRC_UNINITIALIZED; \
      DDCA_Status psc = 0; \
      Display_Ref * dref = (Display_Ref *) ddca_dref; \
      if (dref == NULL || memcmp(dref->marker, DISPLAY_REF_MARKER, 4) != 0 )  { \
         psc = DDCRC_ARG; \
      } \
      else { \
         (action); \
      } \
      return psc; \
   } while(0);


#define WITH_DH(_ddca_dh_, _action_) \
   do { \
      if (!library_initialized) \
         return DDCRC_UNINITIALIZED; \
      DDCA_Status psc = 0; \
      Display_Handle * dh = (Display_Handle *) _ddca_dh_; \
      if ( !dh || memcmp(dh->marker, DISPLAY_HANDLE_MARKER, 4) != 0 )  { \
         psc = DDCRC_ARG; \
      } \
      else { \
         (_action_); \
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



#endif /* API_DISPLAYS_INTERNAL_H_ */
