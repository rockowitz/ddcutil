/* gomain.c
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
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

#include <assert.h>
#include <stdio.h>

#include <base/core.h>

#include "gobject_api/ddcg_gobjects.h"


#define DDC_ERRMSG(function_name,status_code) \
   printf("(%s) %s() returned %d (%s): %s\n",      \
          __func__, function_name, status_code,    \
          ddct_status_code_name(status_code),      \
          ddct_status_code_desc(status_code))


void check_gerror(char * funcname, GError * err) {
   if (err) {
      printf("%s returned GError, domain=%s, code=%d\n",
             funcname,
             g_quark_to_string(err->domain), err->code
            );
      printf("%s\n", err->message);
      exit(1);
   }
}



int main(int argc, char** argv) {
   printf("(%s) Starting.\n", __func__);

   gint x = ddcg_context_get_type();
   printf("ddcg_context_get_type() returned %d\n", x);

   DdcgContext * context = g_object_new(DDCG_TYPE_CONTEXT, NULL);
   printf("context=%p\n", context);

   int max_max_tries = ddcg_context_get_max_max_tries(context);
   printf("(%s) max_max_tries = %d\n", __func__, max_max_tries);


   int busno = 5;
   GError * err = NULL;
   DdcgDisplayIdentifier* ddcg_did = ddcg_display_identifier_create_busno_identifier(busno, &err);
   check_gerror("ddcg_display_identifier_create_busno_identifier", err);
   ddcg_display_identifier_report(ddcg_did, 0);
   printf("After ddcg_display_identifier_report()\n");

   char * repr = ddcg_display_identifier_repr(ddcg_did, &err);
   check_gerror("ddcg_display_identifier_create_busno_identifier", err);
   printf("repr: %s\n", repr);

   DdcgDisplayRef * ddcg_dref = ddcg_display_ref_get(ddcg_did, &err);
   check_gerror("ddcg_display_ref_get", err);
   repr = ddcg_display_ref_repr(ddcg_dref, &err);
   check_gerror("ddcg_display_ref_repr", err);
   printf("repr: %s\n", repr);

   DdcgDisplayHandle * ddcg_dh = ddcg_display_handle_open(ddcg_dref, &err);
   printf("wolf 5\n");
   check_gerror("ddcg_display_handle_open", err);
   repr = ddcg_display_handle_repr(ddcg_dh, &err);
   check_gerror("ddcg_display_handle_repr", err);
   printf("repr: %s\n", repr);

   DdcgContResponse * ddcg_cont_resp = ddcg_display_handle_get_nontable_vcp_value(ddcg_dh, 0x10, &err);
   check_gerror("ddcg_display_handle_get_nontable_vcp_value", err);
   // repr = ddcg_display_handle_cont_resp_repr(ddcg_cont_resp, &err);
   // check_gerror("ddcg_display_cont_resp_repr", err);
   // DBGMSG("repr: %s", repr);
   ddcg_cont_response_report(ddcg_cont_resp, 0);


   return 0;
}
