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
#include "../src/api.c"

#include <string.h>
#include "test.h"

static void convert_hash(dagdb_key key, char* str) {
	int i;
	for(i=0; i<DAGDB_KEY_LENGTH; i++) {
		sprintf(str+2*i, "%02x", key[i]);
	}
}

///////////////////////////////////////////////////////////////////////////////

static void test_hashing() {
	char empty_hash[] = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
	char our_hash[42];
	dagdb_hash h;
	dagdb_data_hash(h, 0, "");
	convert_hash(h, our_hash);
	EX_ASSERT_EQUAL_STRING(our_hash, empty_hash)
}

static CU_TestInfo test_api_non_io[] = {
  { "hashtest", test_hashing },
  CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

static CU_TestInfo test_api_read_write[] = {
  CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

static CU_TestInfo test_api_iterators[] = {
  CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

CU_SuiteInfo api_suites[] = {
	{ "api-non-io",     NULL,        NULL,     test_api_non_io },
	{ "api-read-write", open_new_db, close_db, test_api_read_write },
	{ "api-iterators",  open_new_db, close_db, test_api_iterators },
	CU_SUITE_INFO_NULL,
};
