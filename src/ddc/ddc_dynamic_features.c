/* ddc_dynamic_features.c
 *
 * <copyright>
 * Copyright (C) 2018 Sanford Rockowitz <rockowitz@minsoft.com>
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


#include <ctype.h>

#include <assert.h>
#include <stddef.h>
#include <glib-2.0/glib.h>
#include <string.h>
#include <wordexp.h>
#include <unistd.h>

#include "ddcutil_types.h"
#include "ddcutil_status_codes.h"

#include "util/error_info.h"
#include "util/edid.h"
#include "util/glib_util.h"
#include "util/string_util.h"
#include "util/file_util.h"

#include "base/core.h"
#include "base/displays.h"
#include "base/dynamic_features.h"

#include "ddc/ddc_dynamic_features.h"



// for now, just use an array of pointers to DDCA_Feature_Metadata

// static DDCA_Feature_Metadata * mfg_feature_info[32];


// array of features needs to be per-monitor

// for each detected monitor, look for files with name of form:
//   MFG_modelName_productCode.MCCS
//   spaces and other special characters converted to "_" in modelName

// search path:
//   local directory
//   XDG search path, use directory .local/share/ddcutil

// once we have a dref, can look for feature definition files

// functions:
//   find_feature_definition_file(char * mfg, char * model, uint16_t product_code)
//   load_feature_definition_file(char * fn)
//        creates an array of DDCA_Feature_Metadata
//   hang it off dref
//      dref needs additional flag DREF_LOCAL_FEATURES_SEARCHED
//
// vcp feature code lookup:
//   need function to add local tables
//      create VCP_Feature_Table_Entry from DDCA_Feature_Metadata
//   modify find_feature_by_hexid() to use local tables
//     problem: need to specify which monitor-specific list of local tables to use
//         function variant that takes aux table as argument?
//         perhaps put this function at ddc level?
//
//




/* static */ DDCA_Feature_Metadata *
get_feature_metadata(
      GHashTable * features,
      uint8_t      feature_code)
{
   DDCA_Feature_Metadata * result =
   g_hash_table_lookup(features, GINT_TO_POINTER(feature_code));
   if (result)
      assert( memcmp(result->marker, DDCA_FEATURE_METADATA_MARKER, 4) == 0);
   return result;
}


/* static */ char *
simple_feature_def_filename(
      const char *  mfg,
      const char *  model_name,
      uint16_t      product_code)
{
   bool debug = false;
   DBGMSF(debug, "Starting. mfg=|%s|, model_name=|%s| product_code=%u",
                 mfg, model_name, product_code);

   assert(mfg);
   assert(model_name);
   char * model_name2 = strdup(model_name);
   for (int ndx = 0; ndx < strlen(model_name2); ndx++) {
      if ( !isalnum(model_name2[ndx]) )
         model_name2[ndx] = '_';
   }

   char * result = gaux_asprintf("%s-%s-%u.mccs", mfg, model_name2, product_code);
   DBGMSF("Returning: |%s|", result);
   return result;
}

//search XDG search path

/* static */ char *
find_feature_def_file(
      const char * simple_fn)
{
   bool debug = true;
   DBGMSF(debug, "Starting.  simple_fn=|%s|", simple_fn);
   char * result = NULL;

   char * paths[] = {
         ".",      // current directory
         "~/.local/share/ddcutil",
         "/usr/local/share/ddcutil",
         "/usr/share/ddcutil"
   };
   int paths_ct = ARRAY_SIZE(paths);

   for (int ndx = 0; ndx < paths_ct; ndx++ ) {
      wordexp_t exp_result;
      int wordexp_flags = 0;
      wordexp_flags = WRDE_SHOWERR;           // TEMP
      wordexp(paths[ndx], &exp_result, wordexp_flags);
      char * epath = exp_result.we_wordv[0];
      char fqnamebuf[PATH_MAX];
      snprintf(fqnamebuf, PATH_MAX, "%s/%s", epath, simple_fn);
      // DBGMSF(debug, "fqnamebuf:  |%s|", fqnamebuf);
      wordfree(&exp_result);
      if (access(fqnamebuf, R_OK) == 0) {
         result = strdup(fqnamebuf);
         break;
      }
   }

   DBGMSF(debug, "Returning: |%s|", result);
   return result;
}


// reads a feature definition file into an array of text lines
Error_Info *
read_feature_definition_file(
      const char *  filename,
      GPtrArray *   lines)
{
   Error_Info * errs = NULL;

   int rc = file_getlines(filename,  lines, false);
   if (rc < 0) {
      char * detail = gaux_asprintf("Error reading file %s", filename);
      errs = errinfo_new2(
            rc,
            detail,
            __func__);
      // TODO: variant of errinfo_new2() that puts detail last, as variable args (detail_fmt, ...)
      free(detail);
   }
   return errs;
}


void check_dynamic_features(Display_Ref * dref) {
   bool debug = true;
   DBGMSF(debug, "Starting. ");
   if ( !(dref->flags & DREF_DYNAMIC_FEATURES_CHECKED) ) {
      // DBGMSF(debug, "DREF_DYNAMIC_FEATURES_CHECKED not yet set");
      dref->dfr = NULL;

      char * simple_fn = simple_feature_def_filename(
                           dref->pedid->mfg_id,
                           dref->pedid->model_name,
                           dref->pedid->product_code);

      char * fqfn = find_feature_def_file(simple_fn);
      if (fqfn) {
         // read file into GPtrArray * lines
         GPtrArray * lines = g_ptr_array_new();
         Error_Info * errs = read_feature_definition_file(fqfn, lines);

         Dynamic_Features_Rec * dfr;

         if (!errs) {
            errs = create_monitor_dynamic_features(
                dref->pedid->mfg_id,
                dref->pedid->model_name,
                dref->pedid->product_code,
                lines,
                fqfn,
                &dfr);
         }
         if (errs) {
            // temp
            errinfo_report(errs, 1);
         }
         else {
            dref->dfr = dfr;
         }
      }

      dref->flags |= DREF_DYNAMIC_FEATURES_CHECKED;
   }
   DBGMSF(debug, "Done.");
}

