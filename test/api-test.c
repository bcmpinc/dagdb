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

/** Writes the key to the given string buffer. */
static void convert_hash(dagdb_key key, char* str) {
	int i;
	for(i=0; i<DAGDB_KEY_LENGTH; i++) {
		sprintf(str+2*i, "%02x", key[i]);
	}
}

/** Parses the L hashes given in str and stores these consecutively in the hash array. */
static void parse_hashes(char* hash, const char* str, int L) {
	int j;
	for(j=0; j<L; j++) {
		uint v;
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
	uint a;
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

static void test_handle_types() {
	dagdb_hash k;
	dagdb_data_hash(k, 0, "");
	
	dagdb_handle d = dagdb_data_create(0,"");
	EX_ASSERT_EQUAL_INT(dagdb_get_handle_type(d), DAGDB_HANDLE_INVALID);
	dagdb_handle t = dagdb_trie_create();
	EX_ASSERT_EQUAL_INT(dagdb_get_handle_type(t), DAGDB_HANDLE_MAP);
	dagdb_handle el = dagdb_element_create(k, d, t);
	EX_ASSERT_EQUAL_INT(dagdb_get_handle_type(el), DAGDB_HANDLE_BYTES);
	dagdb_handle kv = dagdb_kvpair_create(el,el);
	EX_ASSERT_EQUAL_INT(dagdb_get_handle_type(kv), DAGDB_HANDLE_INVALID);
	dagdb_handle el2 = dagdb_element_create(k, kv, t);
	EX_ASSERT_EQUAL_INT(dagdb_get_handle_type(el2), DAGDB_HANDLE_INVALID);
	dagdb_handle el3 = dagdb_element_create(k, t, t);
	EX_ASSERT_EQUAL_INT(dagdb_get_handle_type(el3), DAGDB_HANDLE_RECORD);
	EX_ASSERT_EQUAL_INT(dagdb_get_handle_type(dagdb_back_reference(el3)), DAGDB_HANDLE_MAP);
}

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
		uint64_t length = strlen(test_data[i]);
		dagdb_handle e = dagdb_find_bytes(length, test_data[i]);
		dagdb_handle h = dagdb_write_bytes(length, test_data[i]);
		dagdb_handle f = dagdb_find_bytes(length, test_data[i]);
		dagdb_handle g = dagdb_write_bytes(length, test_data[i]);
		EX_ASSERT_EQUAL_INT(e,0);
		CU_ASSERT(h != 0);
		EX_ASSERT_EQUAL_INT(f,h);
		EX_ASSERT_EQUAL_INT(g,h);
		EX_ASSERT_EQUAL_INT(dagdb_get_handle_type(g), DAGDB_HANDLE_BYTES);
		EX_ASSERT_EQUAL_INT(dagdb_bytes_length(g), length);
		
		uint8_t * buffer = malloc(length+11);
		buffer[length]='#';
		uint64_t read = dagdb_bytes_read(buffer, g, 0u, length+10);
		EX_ASSERT_EQUAL_INT(read, length);
		CU_ASSERT(memcmp(buffer, test_data[i], length)==0);
		EX_ASSERT_EQUAL_INT(buffer[length],'#');
		buffer[10]='#';
		read = dagdb_bytes_read(buffer, g, 10u, 10u);
		EX_ASSERT_EQUAL_INT(read, length<10?0:length>20?10:length-10);
		EX_ASSERT_EQUAL_INT(buffer[length],'#');
		if (length>=20) {
			CU_ASSERT(memcmp(buffer, test_data[i], 10)==0);
		}
		free(buffer);
	}
	
	char nullbytes[] = "data with\0null\0bytes.";
	int length = 21;
	{
		dagdb_handle e = dagdb_find_bytes(length, nullbytes);
		dagdb_handle h = dagdb_write_bytes(length, nullbytes);
		dagdb_handle f = dagdb_find_bytes(length, nullbytes);
		dagdb_handle g = dagdb_write_bytes(length, nullbytes);
		EX_ASSERT_EQUAL_INT(e,0);
		CU_ASSERT(h != 0);
		EX_ASSERT_EQUAL_INT(f,h);
		EX_ASSERT_EQUAL_INT(g,h);
		EX_ASSERT_EQUAL_INT(dagdb_get_handle_type(g), DAGDB_HANDLE_BYTES);
		EX_ASSERT_EQUAL_INT(dagdb_bytes_length(g), length);
	}

	verify_chunk_table();
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
		refs[i] = dagdb_write_bytes(1, "abcdefghij" + i);
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
		refs[i] = dagdb_write_bytes(1, "abcdefghij" + i);
		EX_ASSERT_EQUAL_INT(dagdb_get_handle_type(refs[i]), DAGDB_HANDLE_BYTES);
	}
	
	dagdb_handle ref0 = dagdb_find_record(5, (dagdb_record_entry*)refs);
	EX_ASSERT_EQUAL_INT(ref0, 0);
	
	dagdb_handle ref1 = dagdb_write_record(5, (dagdb_record_entry*)refs);
	CU_ASSERT(ref1>0);

	dagdb_handle ref1b = dagdb_write_record(3, (dagdb_record_entry*)refs);
	CU_ASSERT(ref1b>0);

	dagdb_handle ref2 = dagdb_write_record(5, (dagdb_record_entry*)refs);
	EX_ASSERT_EQUAL_INT(ref1, ref2);
	
	dagdb_handle ref3 = dagdb_find_record(5, (dagdb_record_entry*)refs);
	EX_ASSERT_EQUAL_INT(ref1, ref3);

	dagdb_handle ref3b = dagdb_find_record(3, (dagdb_record_entry*)refs);
	EX_ASSERT_EQUAL_INT(ref1b, ref3b);
	
	EX_ASSERT_EQUAL_INT(dagdb_get_handle_type(ref3), DAGDB_HANDLE_RECORD);
	EX_ASSERT_EQUAL_INT(dagdb_get_handle_type(ref3b), DAGDB_HANDLE_RECORD);

	// Select in record checks
	for (i=0; i<5; i++) {
		dagdb_handle key = refs[i*2];
		dagdb_handle val = dagdb_select(ref1, key);
		EX_ASSERT_EQUAL_INT(val, refs[i*2+1]);
		dagdb_handle val2 = dagdb_select(ref3b, key);
		if (i<3) {
			EX_ASSERT_EQUAL_INT(val2, refs[i*2+1]);
		} else {
			EX_ASSERT_EQUAL_INT(val2, 0);
		}
		dagdb_handle val3 = dagdb_select(key, key);
		EX_ASSERT_EQUAL_INT(val3, 0);
	}

	// Backref checks
	for (i=0; i<5; i++) {
		dagdb_handle val = refs[i*2+1];
		dagdb_handle br = dagdb_back_reference(val);
		CU_ASSERT(br != 0);
		EX_ASSERT_EQUAL_INT(dagdb_get_handle_type(br), DAGDB_HANDLE_MAP);
		EX_ASSERT_EQUAL_INT(dagdb_back_reference(br), 0);
		dagdb_handle key = refs[i*2];
		dagdb_handle key_br = dagdb_select(br, key);
		CU_ASSERT(key_br != 0);
		EX_ASSERT_EQUAL_INT(dagdb_select(key_br, ref1), ref1);
		if (i<3) {
			EX_ASSERT_EQUAL_INT(dagdb_select(key_br, ref3b), ref3b);
		} else {
			EX_ASSERT_EQUAL_INT(dagdb_select(key_br, ref3b), 0);
		}
		int j;
		for (j=0; j<10; j++) {
			if (i*2!=j) {
				EX_ASSERT_EQUAL_INT(dagdb_select(br, refs[j]), 0);
			}
		}
	}
	
	verify_chunk_table();
}

static CU_TestInfo test_api_read_write[] = {
	{ "handle_types", test_handle_types },
	{ "data_write", test_data_write },
	{ "record_hash", test_record_hashing },
	{ "record_write", test_record_write },
	CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

static void test_iterator_create() {
	dagdb_handle refs[10];
	int i;
	for(i=0; i<10; i++) {
		refs[i] = dagdb_write_bytes(1, "abcdefghij" + i);
		EX_ASSERT_EQUAL_INT(dagdb_get_handle_type(refs[i]), DAGDB_HANDLE_BYTES);
	}
	dagdb_handle ref1 = dagdb_write_record(0, NULL);
	dagdb_handle ref2 = dagdb_write_record(5, (dagdb_record_entry*)refs);
	dagdb_iterator * it1 = dagdb_iterator_create(ref1);
	dagdb_iterator * it2 = dagdb_iterator_create(ref2);
	dagdb_iterator * it3 = dagdb_iterator_create(refs[0]);
	dagdb_iterator * it4 = dagdb_iterator_create(dagdb_back_reference(ref1));
	
	CU_ASSERT(it1!=0);
	CU_ASSERT(it2!=0);
	CU_ASSERT(it3==0);
	CU_ASSERT(it4!=0);
	
	dagdb_iterator_destroy(it1);
	dagdb_iterator_destroy(it2);
	dagdb_iterator_destroy(it3);
	dagdb_iterator_destroy(it4);
}

static CU_TestInfo test_api_iterators[] = {
	{ "Iterator_create", test_iterator_create },
	CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

CU_SuiteInfo api_suites[] = {
	{ "api-non-io",     NULL,        NULL,     test_api_non_io },
	{ "api-read-write", open_new_db, close_db, test_api_read_write },
	{ "api-iterators",  open_new_db, close_db, test_api_iterators },
	CU_SUITE_INFO_NULL,
};
