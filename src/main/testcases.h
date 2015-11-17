/*  testcases.h
 *
 *  Created on: Oct 27, 2015
 *      Author: rock
 *
 *  Manages test cases
 */

#ifndef TESTCASES_H_
#define TESTCASES_H_

#include <stdbool.h>

#include <base/displays.h>

void showTestCases();
bool execute_testcase(int testnum, Display_Identifier* pdid);

#endif /* TESTCASES_H_ */
