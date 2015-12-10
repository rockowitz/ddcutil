/*  testcase_table.h
 *
 *  Created on: Nov 29, 2015
 *      Author: rock
 */

#ifndef TESTCASE_TABLE_H_
#define TESTCASE_TABLE_H_

#include <base/displays.h>

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
} Testcase_Descriptor;

extern Testcase_Descriptor testcase_catalog[];
extern int testcase_catalog_ct;

// Testcase_Descriptor ** get_testcase_catalog();
// int get_testcase_catalog_ct();

#endif /* TESTCASE_TABLE_H_ */
