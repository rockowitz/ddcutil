// api_displays.h

// Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

 

#ifndef API_DISPLAYS_H_
#define API_DISPLAYS_H_



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

#endif /* API_DISPLAYS_H_ */
