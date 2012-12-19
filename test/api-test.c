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

static void parse_hash(dagdb_hash h, char* str) {
	int i;
	for(i=0; i<DAGDB_KEY_LENGTH; i++) {
		int v;
		sscanf(str+2*i, "%02x", &v);
		h[i]=v;
	}
}

///////////////////////////////////////////////////////////////////////////////

static void test_data_hashing() {
	const char empty_hash[] = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
	const char dagdb_hashed[] = "33b7a80fa95e8f6e70820af2fc4eaf857b9b8c3d";
	char our_hash[42];
	dagdb_hash h;

	dagdb_data_hash(h, 0, "");
	convert_hash(h, our_hash);
	EX_ASSERT_EQUAL_STRING(our_hash, empty_hash)
	
	dagdb_data_hash(h, 5, "dagdb");
	convert_hash(h, our_hash);
	EX_ASSERT_EQUAL_STRING(our_hash, dagdb_hashed)
}

static void test_record_sorting() {
}

static void test_record_hashing() {
}

static CU_TestInfo test_api_non_io[] = {
  { "data_hash", test_data_hashing },
  { "record_sorting", test_record_sorting },
  { "record_hash", test_record_hashing },
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
