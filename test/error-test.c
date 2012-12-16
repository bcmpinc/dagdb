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

// include the entire file being tested.
#include "../src/error.c"

#include "test.h"

static void test_no_error() {
	EX_ASSERT_NO_ERROR
	CU_ASSERT_NOT_EQUAL(dagdb_last_error(), NULL);
}

static void test_p() {
	errno = 12; // Cannot allocate memory error.
	dagdb_report_p("%d %d", 25, 42);
	CU_ASSERT(strstr(dagdb_last_error(),__func__)!=NULL);
	CU_ASSERT(strstr(dagdb_last_error(),"25 42")!=NULL);
	CU_ASSERT(strstr(dagdb_last_error(),"Cannot allocate memory")!=NULL);
	errno = 0;
}

static void test_long_function() {
	char fn[1024];
	int i;
	for(i=0; i<1000; i++) fn[i]='A';
	fn[1000]=0;
	
	dagdb_report_int(fn, "%s %d", "test", 12345);
	CU_ASSERT(strstr(dagdb_last_error(),"AAAAAAAAAA")!=NULL);
	dagdb_report_p_int(fn, "%s %d", "test", 12345);
	CU_ASSERT(strstr(dagdb_last_error(),"AAAAAAAAAA")!=NULL);
}

static void test_long_message() {
	char b[128];
	int i;
	for(i=0; i<100; i++) b[i]='B';
	b[100]=0;
	
	dagdb_report("%s %s %s %s %s %s %s %s", b, b, b, b, b, b, b, b);
	CU_ASSERT(strstr(dagdb_last_error(),"BBBBBBBBBB BBBBBBBBBB")!=NULL);
	dagdb_report_p("%s %s %s %s %s %s %s %s", b, b, b, b, b, b, b, b);
	CU_ASSERT(strstr(dagdb_last_error(),"BBBBBBBBBB BBBBBBBBBB")!=NULL);
}

static CU_TestInfo test_error[] = {
	{ "no_errors", test_no_error },
	{ "perror", test_p },
//	{ "long_function", test_long_function }, // TODO: figure out why this kills gcov.
	{ "long_message", test_long_message },
	CU_TEST_INFO_NULL,
};

CU_SuiteInfo error_suites[] = {
	{ "error",   NULL,        NULL,     test_error },
	CU_SUITE_INFO_NULL,
};
