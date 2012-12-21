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

static void parse_hashes(char* hash, const char* str, int L) {
	int j;
	for(j=0; j<L; j++) {
		int v;
		sscanf(str+2*j, "%02x", &v);
		hash[j]=v;
	}
}

static void write_hashes(const char* hash, char* str, int L) {
	int j;
	for(j=0; j<L; j++) {
		sprintf(str+2*j, "%02x", (unsigned char)hash[j]);
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

/* A test record. Its contents are:
 * a -> b
 * c -> d
 * e -> f
 * g -> h
 * i -> j
 */

const char record[] =
	"86f7e437faa5a7fce15d1ddcb9eaeaea377667b8e9d71f5ee7c92d6dc9e92ffdad17b8bd49418f98"
	"84a516841ba77a5b4648de2cd0dfcb30ea46dbb43c363836cf4e16666669a25da280a1865c2d2874"
	"58e6b3a414a1e090dfc6029add0f3555ccba127f4a0a19218e082a343a1b17e5333409af9d98f0f5"
	"54fd1711209fb1c0781092374132c66e79e2241b27d5482eebd075de44389774fce28c69f45c8a75"
	"042dc4512fa3d391c5170cf3aa61e6a638f843425c2dd944dde9e08881bef0894fe7b22a5c9c4b06";

const char record_sorted[] =
	"042dc4512fa3d391c5170cf3aa61e6a638f843425c2dd944dde9e08881bef0894fe7b22a5c9c4b06"
	"54fd1711209fb1c0781092374132c66e79e2241b27d5482eebd075de44389774fce28c69f45c8a75"
	"58e6b3a414a1e090dfc6029add0f3555ccba127f4a0a19218e082a343a1b17e5333409af9d98f0f5"
	"84a516841ba77a5b4648de2cd0dfcb30ea46dbb43c363836cf4e16666669a25da280a1865c2d2874"
	"86f7e437faa5a7fce15d1ddcb9eaeaea377667b8e9d71f5ee7c92d6dc9e92ffdad17b8bd49418f98";

/* 
 * This test checks if the hash comparison function works properly.
 */
static void test_record_sorting() {
	// Check if sscanf works as expected.
	int a;
	sscanf("123","%02x",&a);
	EX_ASSERT_EQUAL_INT(18, a);
	
	// Convert the hex-encoded strings
	const int L = sizeof(record) / 2;
	char temp[L], temp_sorted[L];
	parse_hashes(temp, record, L);
	parse_hashes(temp_sorted, record_sorted, L);
	
	// Sort and check
	qsort(temp, 5, 2*DAGDB_KEY_LENGTH, cmphash);
	CU_ASSERT(memcmp(temp,temp_sorted,L)==0);
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
