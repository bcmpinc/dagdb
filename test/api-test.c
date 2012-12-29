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

const char empty_hash[] = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
const char dagdb_hashed[] = "33b7a80fa95e8f6e70820af2fc4eaf857b9b8c3d";

static void test_data_hashing() {
	char our_hash[42];
	dagdb_hash h;

	dagdb_data_hash(h, 0, "");
	convert_hash(h, our_hash);
	EX_ASSERT_EQUAL_STRING(our_hash, empty_hash)
	
	dagdb_data_hash(h, 5, "dagdb");
	convert_hash(h, our_hash);
	EX_ASSERT_EQUAL_STRING(our_hash, dagdb_hashed)
}

static void test_hash_flip() {
	dagdb_hash h;
	dagdb_data_hash(h, 0, "");
	dagdb_hash g;
	memcpy(g,h,DAGDB_KEY_LENGTH);
	flip_hash(g);
	CU_ASSERT_NSTRING_NOT_EQUAL(g,h, DAGDB_KEY_LENGTH);
	flip_hash(g);
	CU_ASSERT_NSTRING_EQUAL(g,h, DAGDB_KEY_LENGTH);
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

const char record_hash_unflipped[] = "7013bbcf8e68c59d8bd5f0c12248edf18b4f2cc3";

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
	
	// Test conversion code
	char buf[L*2+1];
	write_hashes(temp, buf, L);
	EX_ASSERT_EQUAL_STRING(buf, record);

	// Sort and check
	qsort(temp, 5, 2*DAGDB_KEY_LENGTH, cmphash);
	CU_ASSERT(memcmp(temp,temp_sorted,L)==0);
}

static CU_TestInfo test_api_non_io[] = {
  { "data_hash", test_data_hashing },
  { "hash_flip", test_hash_flip },
  { "record_sorting", test_record_sorting },
  CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

const char * test_data[] = {
	"",
	"a",
	"b",
	"test",
	"slightly longer data",
	record,
	record_sorted,
};

static void test_data_write() {
	const int N = sizeof(test_data) / sizeof(test_data[0]);
	int i;
	for (i=0; i<N; i++) {
		dagdb_handle e = dagdb_find_data(strlen(test_data[i]), test_data[i]);
		dagdb_handle h = dagdb_write_data(strlen(test_data[i]), test_data[i]);
		dagdb_handle f = dagdb_find_data(strlen(test_data[i]), test_data[i]);
		dagdb_handle g = dagdb_write_data(strlen(test_data[i]), test_data[i]);
		EX_ASSERT_EQUAL_INT(e,0);
		CU_ASSERT(h != 0);
		EX_ASSERT_EQUAL_INT(f,h);
		EX_ASSERT_EQUAL_INT(g,h);
	}
	
	char nullbytes[] = "data with\0null\0bytes.";
	int length = 21;
	{
		dagdb_handle e = dagdb_find_data(length, nullbytes);
		dagdb_handle h = dagdb_write_data(length, nullbytes);
		dagdb_handle f = dagdb_find_data(length, nullbytes);
		dagdb_handle g = dagdb_write_data(length, nullbytes);
		EX_ASSERT_EQUAL_INT(e,0);
		CU_ASSERT(h != 0);
		EX_ASSERT_EQUAL_INT(f,h);
		EX_ASSERT_EQUAL_INT(g,h);
	}
}

static void test_record_hashing() {
	// Convert the hex-encoded strings
	const int L = sizeof(record) / 2;
	char temp[L], temp_sorted[L];
	parse_hashes(temp, record, L);
	parse_hashes(temp_sorted, record_sorted, L);
	
	// Test Hash of pre-sorted data
	char our_hash[42];
	dagdb_hash h;
	dagdb_data_hash(h, 5 * 2 * DAGDB_KEY_LENGTH, temp_sorted);
	convert_hash(h, our_hash);
	EX_ASSERT_EQUAL_STRING(our_hash, record_hash_unflipped);
	
	// Test record hash function.
	dagdb_handle refs[10];
	int i;
	for(i=0; i<10; i++) {
		refs[i] = dagdb_write_data(1, "abcdefghij" + i);
	}
	
	dagdb_record_hash(h, 5, (dagdb_record_entry*)refs);
	flip_hash(h);
	convert_hash(h, our_hash);
	EX_ASSERT_EQUAL_STRING(our_hash, record_hash_unflipped);
}

static void test_record_write() {
	dagdb_handle refs[10];
	int i;
	for(i=0; i<10; i++) {
		refs[i] = dagdb_write_data(1, "abcdefghij" + i);
	}
	
	dagdb_handle ref1 = dagdb_write_record(5, (dagdb_record_entry*)refs);
	CU_ASSERT(ref1>0);
	
	dagdb_handle ref2 = dagdb_write_record(5, (dagdb_record_entry*)refs);
	EX_ASSERT_EQUAL_INT(ref1, ref2);
	
	// TODO: perform more checks.
}

static CU_TestInfo test_api_read_write[] = {
  { "data_write", test_data_write },
  { "record_hash", test_record_hashing },
  { "record_write", test_record_write },
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
