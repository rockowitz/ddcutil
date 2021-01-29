// app_testcases.c

// Copyright (C) 2014-2021 Sanford Rockowitz <rockowitz@minsoft.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stdio.h>

#include "ddcutil_status_codes.h"

#include "util/error_info.h"
#include "base/core.h"
#include "cmdline/parsed_cmd.h"
#include "ddc/ddc_displays.h"
#include "ddc/ddc_vcp.h"
#include "test/testcases.h"
#include "app_ddcutil/app_testcases.h"

bool
app_testcases(Parsed_Cmd* parsed_cmd)
{
   int testnum;
   bool ok = true;
   int ct = sscanf(parsed_cmd->args[0], "%d", &testnum);
   if (ct != 1) {
      f0printf(fout(), "Invalid test number: %s\n", parsed_cmd->args[0]);
      ok = false;
   }
   else {
      ddc_ensure_displays_detected();
      ok = true;
      // Why is ddc_save_current_settings() call here?
#ifdef OUT
      Error_Info * ddc_excp = ddc_save_current_settings(dh);
      if (ddc_excp)  {
         f0printf(fout(), "Save current settings failed. rc=%s\n", psc_desc(ddc_excp->status_code));
         if (ddc_excp->status_code == DDCRC_RETRIES)
            f0printf(fout(), "    Try errors: %s", errinfo_causes_string(ddc_excp) );
         errinfo_report(ddc_excp, 0);   // ** ALTERNATIVE **/
         errinfo_free(ddc_excp);
         // ERRINFO_FREE_WITH_REPORT(ddc_excp, report_exceptions);
         ok = false;
      }
      if (ok) {
#endif
         if (!parsed_cmd->pdid)
            parsed_cmd->pdid = create_dispno_display_identifier(1);   // default monitor
         ok = execute_testcase(testnum, parsed_cmd->pdid);
#ifdef OUT
      }
#endif
   }

return ok;
}
