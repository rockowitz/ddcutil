/*  testcase_table.c
 *
 *  Created on: Nov 29, 2015
 *      Author: rock
 */

#include <config.h>

#include "base/displays.h"

#include "test/ddc/ddc_capabilities_tests.h"
#include "test/ddc/ddc_vcp_tests.h"
#include "test/i2c/i2c_edid_tests.h"
#ifdef HAVE_ADL
#include "test/adl/adl_tests.h"
#endif


#include "test/testcase_table.h"

Testcase_Descriptor testcase_catalog[] = {
      {"get_luminosity_sample_code",        DisplayRefBus,  NULL, get_luminosity_sample_code, NULL, NULL},
#ifdef HAVE_ADL
      {"adl_testmain",                      DisplayRefNone, adl_testmain, NULL, NULL, NULL},
      {"diddleBrightness",                  DisplayRefAdl,  NULL, NULL, diddle_adl_brightness,  NULL},
      {"exercise_ad_calls",                 DisplayRefAdl,  NULL, NULL, exercise_ad_calls, NULL},
      {"run_adapter_display_tests",         DisplayRefNone, run_adapter_display_tests, NULL, NULL, NULL},
#endif
      {"get_luminosity_using_single_ioctl", DisplayRefBus,  NULL, get_luminosity_using_single_ioctl, NULL, NULL},
      {"demo_nvidia_bug_sample_code",       DisplayRefBus,  NULL, demo_nvidia_bug_sample_code, NULL, NULL},
      {"demo_p2411_problem",                DisplayRefBus,  NULL, demo_p2411_problem, NULL, NULL}

};
int testcase_catalog_ct = sizeof(testcase_catalog)/sizeof(Testcase_Descriptor);

