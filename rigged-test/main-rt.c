/*
    DagDB - A lightweight structured database system.
    Copyright (C) 2012  B.J. Conijn <bcmpinc@users.sourceforge.net>

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

extern CU_SuiteInfo api_rt1_suites[];

int main() {//int argc, char **argv) {
	printf("Testing DAGDB\n");
	CU_initialize_registry();
	//CU_basic_set_mode(CU_BRM_SILENT);
	CU_basic_set_mode(CU_BRM_NORMAL);
	//CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_register_suites(api_rt1_suites);
	CU_basic_run_tests();
	int result = CU_get_number_of_tests_failed();
	CU_cleanup_registry();
	return result; 
}
