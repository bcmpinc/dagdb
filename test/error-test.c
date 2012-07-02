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

#include "cunit-extensions.h"

static void test_no_error() {
	EX_ASSERT_NO_ERROR
	CU_ASSERT_NOT_EQUAL(dagdb_last_error(), NULL);
}

static CU_TestInfo test_error[] = {
  { "no_errors", test_no_error },
  CU_TEST_INFO_NULL,
};

CU_SuiteInfo error_suites[] = {
	{ "error",   NULL,        NULL,     test_error },
	CU_SUITE_INFO_NULL,
};
