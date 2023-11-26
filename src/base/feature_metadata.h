/* @file feature_metadata.h
 *
 * Functions for external and internal representation of
 * display-specific feature metadata.
 */

// Copyright (C) 2018-2023 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FEATURE_METADATA_H_
#define FEATURE_METADATA_H_

/** \cond */
#include <glib-2.0/glib.h>
#include <stdbool.h>
/** \endcond */

#include "ddcutil_types.h"

#include "util/data_structures.h"

#include "base/dynamic_features.h"


/** Simple stripped-down version of Parsed_Nontable_Vcp_Response */
typedef
struct {
   DDCA_Vcp_Feature_Code   vcp_code;
   gushort max_value;
   gushort cur_value;
   // for new way
   Byte   mh;
   Byte   ml;
   Byte   sh;
   Byte   sl;
} Nontable_Vcp_Value;

char * nontable_vcp_value_repr_t(Nontable_Vcp_Value * vcp_value);

// Prototypes for functions that format feature values

typedef
bool (*Format_Normal_Feature_Detail_Function) (
          Nontable_Vcp_Value*     code_info,
          DDCA_MCCS_Version_Spec  vcp_version,
          char *                  buffer,
          int                     bufsz);

typedef
bool (*Format_Normal_Feature_Detail_Function2) (
          Nontable_Vcp_Value*        code_info,
       // Display_Ref *              dref,
       // DDCA_MCCS_Version_Spec     vcp_version,
          DDCA_Feature_Value_Entry * sl_values,
          char *                     buffer,
          int                        bufsz);

typedef
bool (*Format_Table_Feature_Detail_Function) (
          Buffer *                data_bytes,
          DDCA_MCCS_Version_Spec  vcp_version,
          char **                 p_result_buffer);


// combines Format_Normal_Feature_Detail_Fucntion, Format_Normal_Detail_Function2
// for future use
typedef
bool (*Format_Normal_Feature_Detail_Function3) (
          Nontable_Vcp_Value*        code_info,
          DDCA_MCCS_Version_Spec     vcp_version,
          DDCA_Feature_Value_Entry * sl_values,
          char *                     buffer,
          int                        bufsz);

//typedef
//bool (*Format_Table_Feature_Detail_Functionx) (
//          Buffer *                data_bytes,
//          DDCA_MCCS_Version_Spec  vcp_version,
//          char **                 p_result_buffer);


// Feature value table functions

void
dbgrpt_sl_value_table(DDCA_Feature_Value_Entry * table, char * title, int depth);

DDCA_Feature_Value_Entry *
copy_sl_value_table(DDCA_Feature_Value_Entry * oldtable);

void
free_sl_value_table(DDCA_Feature_Value_Entry * table);

char *
sl_value_table_lookup(DDCA_Feature_Value_Entry * value_entries, Byte value_id);


// Feature Flags

char *
interpret_feature_flags_t(DDCA_Version_Feature_Flags flags);


// DDCA_Feature_Metadata

void
dbgrpt_ddca_feature_metadata(DDCA_Feature_Metadata * md, int depth);


void
dbgrpt_dyn_feature_metadata(Dyn_Feature_Metadata * md, int depth);

void
free_ddca_feature_metadata(DDCA_Feature_Metadata * metadata);


// Display_Feature_Metadata

#define DISPLAY_FEATURE_METADATA_MARKER "DFMD"
/** Internal version of display specific feature metadata, includes formatting functions
 *
 *  Represents merged internal metadata from vcp_code_tables.c, synthetic metadata,
 *  and user defined features, for a specific VCP version.
 * */
typedef
struct {
   char                                    marker[4];
   DDCA_Display_Ref                        display_ref;    // needed?
   DDCA_Vcp_Feature_Code                   feature_code;
   DDCA_MCCS_Version_Spec                  vcp_version;    // needed - yes, used in ddcui
   char *                                  feature_name;
   char *                                  feature_desc;
   DDCA_Feature_Value_Entry *              sl_values;     /**< valid when DDCA_SIMPLE_NC set */
   DDCA_Feature_Flags                      feature_flags;
   Format_Normal_Feature_Detail_Function   nontable_formatter;
   Format_Normal_Feature_Detail_Function2  nontable_formatter_sl;
   Format_Normal_Feature_Detail_Function3  nontable_formatter_universal;   // the future
   Format_Table_Feature_Detail_Function    table_formatter;
} Display_Feature_Metadata;

void
dbgrpt_display_feature_metadata(Display_Feature_Metadata * meta, int depth);

void
dfm_free(Display_Feature_Metadata * meta);

Display_Feature_Metadata *
dfm_new(DDCA_Vcp_Feature_Code feature_code);

#ifdef UNUSED
void  dfm_set_feature_name(Display_Feature_Metadata * meta, const char * feature_name);
void  dfm_set_feature_desc(Display_Feature_Metadata * meta, const char * feature_desc);
#endif

// Conversion functions

DDCA_Feature_Metadata *
dfm_to_ddca_feature_metadata(Display_Feature_Metadata * dfm);

Display_Feature_Metadata *
dfm_from_dyn_feature_metadata(Dyn_Feature_Metadata * meta);

void init_feature_metadata();

#endif /* FEATURE_METADATA_H_ */
