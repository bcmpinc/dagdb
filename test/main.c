/*
    DagDB - A lightweight structured database system.
    Copyright (C) 2011  B.J. Conijn <bcmpinc@users.sourceforge.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h> 

/** @file
 * @brief Entry point of the unit tests.
 */
extern CU_SuiteInfo base2_suites[];

int main(int argc, char **argv) {
	printf("Testing DAGDB\n");
	CU_initialize_registry();
	int i=0;
	while(base2_suites[i].pName) {
		CU_pSuite s = CU_add_suite(base2_suites[i].pName,base2_suites[i].pInitFunc,base2_suites[i].pCleanupFunc);
		int j=0;
		CU_TestInfo * tests = base2_suites[i].pTests;
		while(tests[j].pName) {
			CU_add_test(s, tests[j].pName, tests[j].pTestFunc);
			j++;
		}
		i++;
	}
	CU_basic_run_tests();
	CU_cleanup_registry();
	return 0; // TODO change such that it is non-zero in case of a failing test.
}
