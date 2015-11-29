/*  testcases.c
 *
 *  Created on: Oct 27, 2015
 *      Author: rock
 *
 *  Manages test cases
 */


#include <stdio.h>

#include <config.h>

#include "base/displays.h"
#include "base/util.h"

#ifdef HAVE_ADL
#include "adl/adl_impl/adl_intf.h"
#include "test/adl/adl_tests.h"
#endif

#include "test/ddc/ddc_capabilities_tests.h"
#include "test/ddc/ddc_vcp_tests.h"
#include "test/i2c/i2c_edid_tests.h"

#include "main/testcases.h"


//
// Testcase dispatching
//

// type of display reference required/supported by the command
typedef enum {DisplayRefNone, DisplayRefAny, DisplayRefBus, DisplayRefAdl} DisplayRefType;

typedef void (*NoArgFunction)();
typedef void (*BusArgFunction)(int busno);
typedef void (*AdlArgFunction)(int iAdapterIndex, int iDisplayIndex);
typedef void (*DisplayRefArgFunction)(Display_Ref * dref);


typedef
struct {
   char *                name;      // testcase description
   DisplayRefType        drefType;
   // should really be a union
   NoArgFunction         fp_noarg;
   BusArgFunction        fp_bus;
   AdlArgFunction        fp_adl;
   DisplayRefArgFunction fp_dr;
} TestcaseDescriptor;


// void adl_testmain();

// void diddleBrightness(int iAdapterIndex, int iDisplayIndex);

static TestcaseDescriptor testCatalog[] = {
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
static int testCatalogCt = sizeof(testCatalog)/sizeof(TestcaseDescriptor);


void showTestCases() {
   printf("\n Test Cases:\n");
   int ndx = 0;
   for (;ndx < testCatalogCt; ndx++) {
      printf("  %d - %s\n", ndx+1, testCatalog[ndx].name);
   }
   puts("");
}


TestcaseDescriptor * getTestcaseDescriptor(int testnum) {
   TestcaseDescriptor * result = NULL;
   if (testnum > 0 && testnum <= testCatalogCt) {
      result = &testCatalog[testnum-1];
   }
   return result;
}

bool execute_testcase(int testnum, Display_Identifier* pdid) {
      bool ok = true;
      TestcaseDescriptor * pDesc = NULL;

      if (ok) {
         pDesc = getTestcaseDescriptor(testnum);
         if (!pDesc) {
            printf("Invalid test number: %d\n", testnum);
            ok = false;
         }
      }

      if (ok) {
#ifdef HAVE_ADL
         if (pdid->id_type == DISP_ID_ADL && !adl_is_available()) {
            printf("ADL adapter.display numbers specified, but ADL is not available.\n");
            ok = false;
         }
#else
         ok = false;
#endif
      }

      if (ok) {
         switch (pDesc->drefType) {

         case DisplayRefNone:
            pDesc->fp_noarg();
            break;

         case DisplayRefBus:
            // if (parsedCmd->dref->ddc_io_mode == DDC_IO_ADL) {
            if (pdid->id_type != DISP_ID_BUSNO) {
               printf("Test %d requires bus number\n", testnum);
               ok = false;
            }
            else {
               // pDesc->fp_bus(parsedCmd->dref->busno);
               pDesc->fp_bus(pdid->busno);
            }
            break;

         case DisplayRefAdl:
             // if (parsedCmd->dref->ddc_io_mode == DDC_IO_DEVI2C) {
             if (pdid->id_type != DISP_ID_ADL) {
                printf("Test %d requires ADL adapter.display numbers\n", testnum);
                ok = false;
             }
             else {
                // pDesc->fp_adl(parsedCmd->dref->iAdapterIndex, parsedCmd->dref->iDisplayIndex);
                pDesc->fp_adl(pdid->iAdapterIndex, pdid->iDisplayIndex);
             }
             break;

         case DisplayRefAny:
            {
               // pDesc->fp_dr(parsedCmd->dref);
               Display_Ref* pdref = NULL;
               if (pdid->id_type == DISP_ID_ADL) {
                  pdref = create_adl_display_ref(pdid->iAdapterIndex, pdid->iDisplayIndex);
               }
               else {
                  pdref = create_bus_display_ref(pdid->busno);
               }
               pDesc->fp_dr(pdref);
            }
            break;

         default:
            PROGRAM_LOGIC_ERROR("Impossible display id type: %d\n", pDesc->drefType);
         }  // switch
      }

     return ok;
}

