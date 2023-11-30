/** @file api_displays_internal.h
 *
 *  For use only by other api_... files.
 */

// Copyright (C) 2015-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later
 

#ifndef API_DISPLAYS_INTERNAL_H_
#define API_DISPLAYS_INTERNAL_H_

#include "base/core.h"
#include "base/displays.h"
#include "public/ddcutil_types.h"

Display_Ref * validated_ddca_display_ref(DDCA_Display_Ref ddca_dref);
DDCA_Status validate_ddca_display_ref(DDCA_Display_Ref ddca_dref, Display_Ref** dref_loc);
Display_Handle * validated_ddca_display_handle(DDCA_Display_Handle ddca_dh);

#ifdef UNUSED
#define VALIDATE_DDCA_DREF(_ddca_dref, _dref, _debug) \
   do { \
      _dref = validated_ddca_display_ref(_ddca_dref); \
      if (!_dref) { \
         DBGTRC_DONE(_debug, DDCA_TRC_API, "Returning DDCRC_ARG"); \
         return DDCRC_ARG; \
      } \
   } while(0)
#endif


#define VALIDATE_DDCA_DREF2(_ddca_dref, _dref, _rc, _debug) \
   do { \
      _dref = validated_ddca_display_ref(_ddca_dref); \
      if (!_dref) { \
         _rc = DDCRC_ARG; \
      } \
   } while(0)


#ifdef OLD
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
#endif

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


#ifdef UNUSED
#define WITH_VALIDATED_DH2(ddca_dh, action) \
   do { \
      assert(library_initialized); \
      DDCA_Status psc = 0; \
      free_thread_error_detail(); \
      Display_Handle * dh = validated_ddca_display_handle(ddca_dh); \
      if (!dh)  { \
         psc = DDCRC_ARG; \
         DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, psc, "ddca_dh=%p", ddca_dh); \
      } \
      else { \
         (action); \
      } \
      /* DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, psc, ""); */ \
      return psc; \
   } while(0);
#endif

#define WITH_VALIDATED_DH3(ddca_dh, _ddcrc, action) \
   do { \
      assert(library_initialized); \
      _ddcrc = 0; \
      free_thread_error_detail(); \
      Display_Handle * dh = validated_ddca_display_handle(ddca_dh); \
      if (!dh)  { \
         _ddcrc = DDCRC_ARG; \
         DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, _ddcrc, "ddca_dh=%p", ddca_dh); \
      } \
      else { \
         (action); \
      } \
      /* DBGTRC_RET_DDCRC(debug, DDCA_TRC_API, psc, ""); */ \
   } while(0);


void init_api_displays();

#endif /* API_DISPLAYS_INTERNAL_H_ */
