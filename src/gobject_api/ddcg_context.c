/* ddcg_context.c
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

#include "base/core.h"
#include "base/status_code_mgt.h"

#include "ddcg_structs.h"
#include "gobject_api/ddcg_context.h"
#include "public/ddcutil_c_api.h"


//
// Build Information
//

#ifdef REF

DDCA_Ddcutil_Version_Spec ddca_ddcutil_version(void);       // ddcutil version

const char * ddca_ddcutil_version_string();
#endif


/**
 * ddcg_get_ddcutil_version_spec:
 *
 * Returns:(transfer none): pointer to struct of ints
 */
DdcgDdcutilVersionSpec * ddcg_get_ddcutil_version_spec(void) {
   DDCA_Ddcutil_Version_Spec vspec = ddca_ddcutil_version();
   static DdcgDdcutilVersionSpec * gvspec = NULL;
   if (!gvspec) {
      gvspec = ddcg_ddcutil_version_spec_new();
      gvspec->major = vspec.major;
      gvspec->minor = vspec.minor;
      gvspec->micro = vspec.micro;
   }

   return gvspec;
}


#ifdef NO
/**
 * ddcg_get_ddcutil_version_spec2:
 *
 * Returns:(transfer none)(array fixed-size=3)(element-type gint)  : pointer to struct of ints
 */
DdcgDdcutilVersionSpec2 ddcg_get_ddcutil_version_spec2(void) {
   DDCA_Ddcutil_Version_Spec vspec = ddca_ddcutil_version();
   gint gvspec[3];
   gvspec[0]  = vspec.major;
   gvspec[1] = vspec.minor;
   gvspec[2] = vspec.micro;

   return gvspec;
}
#endif


#ifdef NO
// makes integer from pointer w/o a cast
/**
 * ddcg_get_ddcutil_version_spec3:
 *
 * Returns:(transfer none)(array fixed-size=3)(element-type gint)  : pointer to struct of ints
 */
gint ddcg_get_ddcutil_version_spec3(void) {
   DDCA_Ddcutil_Version_Spec vspec = ddca_ddcutil_version();
   gint gvspec[3];
   gvspec[0]  = vspec.major;
   gvspec[1] = vspec.minor;
   gvspec[2] = vspec.micro;

   return gvspec;
}
#endif


/**
 * ddcg_get_ddcutil_version_spec4:
 *
 * Returns:(transfer none)(array fixed-size=3)(element-type gint)  : pointer to struct of ints
 */
gint * ddcg_get_ddcutil_version_spec4(void) {
   static gint gvspec[3];
   DDCA_Ddcutil_Version_Spec vspec = ddca_ddcutil_version();
   gvspec[0] = vspec.major;
   gvspec[1] = vspec.minor;
   gvspec[2] = vspec.micro;

   return gvspec;
}


/**
 * ddcg_get_ddcutil_version_string:
 * Returns: (transfer none): ddcutil version as a string
 */
const gchar * ddcg_get_ddcutil_version_string(void) {
   return ddca_ddcutil_version_string();
}


/**
 * ddcg_get_build_options:
 * Returns: build flags
 */
const guint8 ddcg_get_build_options(void) {
   return ddca_get_build_options();
}


//
// Status Codes
//

/**
 * ddcg_rc_name:
 * @status_code: numeric status code
 *
 * Returns: symbolic name, e.g. EBUSY, DDCRC_INVALID_DATA
 *
 * Returns the symbolic name for a ddcutil status code
 */
const gchar * ddcg_rc_name(DdcgStatusCode status_code) {
   return ddca_rc_name(status_code);
}

/**
 * ddcg_rc_desc:
 * @status_code numeric status code
 *
 * Returns: explanation of status code, e.g. "device or resource busy"
 *
 *  Returns explanation of status code
 */
const gchar * ddcg_rc_desc(DdcgStatusCode status_code) {
   return ddca_rc_desc(status_code);
}


#ifdef DOESNT_WORK
struct _DdcgContextClass {
   GObjectClass parent_class;
   bool     class_initialized;
};
#endif

struct _DdcgContext {
   GObject   parent_instance;
};


G_DEFINE_TYPE(DdcgContext, ddcg_context, G_TYPE_OBJECT)

static void ddcg_context_class_init(DdcgContextClass * cls);
static void ddcg_context_init(DdcgContext * context);
#ifdef UNUSED
static void ddcg_context_finalize(GObject * obj);
#endif

#ifdef NO
static void ddcg_context_constructed(GObject * obj) {
   DBGMSG("Starting");
   // Update the object state depending on constructor properties

   // Chain up to parent constructed function to complete object initialization
   G_OBJECT_CLASS(ddcg_context)->parent_class->constructed(obj));

}
#endif


static void ddcg_context_class_init(DdcgContextClass * cls) {
   DBGMSG("Starting");
#ifdef WONT_COMPILE
   GObjectClass * object_class = G_OBJECT_CLASS(cls);
   object_class->constructed = ddcg_context_constructed;
#endif
   // ddca_init();
   // cls->class_initialized = true;  // no member named class_initialized
}


static void ddcg_context_init(DdcgContext * context) {
   DBGMSG("Starting");
   // initialize the instance
}


#ifdef UNUSED
static void ddcg_context_finalize(GObject * obj) {
}
#endif

DdcgContext * ddcg_context_new(void) {
   return g_object_new(DDCG_TYPE_CONTEXT, NULL);
}


gint32 ddcg_context_get_max_max_tries(
      DdcgContext * ddcg_context)
    //  GError **     error)
{
   return ddca_get_max_max_tries();

}


/**
 * ddcg_context_get_max_tries:
 * @ddcg_context  context
 * @retry_type: retry type
 *
 * Returns:  max tries for specified type
 */
gint32
ddcg_context_get_max_tries(
      DdcgContext *  ddcg_context,
      DdcgRetryType retry_type) {
   return ddca_get_max_tries(retry_type);
}


/**
 * ddcg_context_set_max_tries:
 * @ddcg_context: context
 * @retry_type:   type of retry
 * @max_tries:    count to set
 * @error: (out): location where to return error
 *
 * Sets the retry count
 */
void
ddcg_context_set_max_tries(
      DdcgContext *  ddcg_context,
      DdcgRetryType retry_type,
      gint32             max_tries,
      GError **          error)
{
   *error = NULL;
   DDCA_Status psc = ddca_set_max_tries(retry_type, max_tries);
   if (psc) {
      GQuark domain = g_quark_from_string("DDCTOOL_DDCG");
      g_set_error(error,  domain, psc, "ddca_set_max_tries() returned ddct_status=%s", psc_desc(psc));
   }
}


/**
 * ddcg_context_create_display_ref:
 * @ddcg_did: display identifier
 * @error: (out): location where to return error
 *
 * Creates a #DdcgDisplayRef from a #DdcgDisplayIdentifier.
 *
 * This may be a direct conversion (for busno or adlno) or may
 * entail searching the list of monitors detected.  If the
 * DisplayIdentifier does not refer to a valid monitor, an
 * error is returned.
 *
 * Returns: (transfer full): new #DdcgDisplayRef
 */
DdcgDisplayRef *
ddcg_context_create_display_ref(
      DdcgDisplayIdentifier * ddcg_did,
      GError **               error)
{
   g_return_val_if_fail (error == NULL || *error == NULL, NULL);

   DDCA_Display_Identifier ddct_did = NULL;   // extract from DdcgDisplayIdentifier
   ddct_did = _ddcg_display_identifier_get_ddct_object(ddcg_did);
   DdcgDisplayRef * ddcg_dref = NULL;
   DDCA_Display_Ref ddct_dref = NULL;     // is pointer
   DDCA_Status ddct_status = ddca_get_display_ref(ddct_did, &ddct_dref);
   if (ddct_status == 0) {
      DdcgDisplayRef * ddcg_dref = ddcg_display_ref_new();
      _ddcg_display_ref_set_ddct_object(ddcg_dref, ddct_dref);
      // also save context?
   }
   else {
      GQuark domain = g_quark_from_string("DDCTOOL_DDCG");
      g_set_error(error,  domain, ddct_status,
                  "invalid display identifier.  ddct_get_display_ref() returned ddct_status=%d", ddct_status);
   }
   return ddcg_dref;
}
