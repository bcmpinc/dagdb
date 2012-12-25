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
#include "../src/base.c"

#include <string.h>
#include "test.h"

/** \file
 * Tests the I/O base system of DagDB.
 * Note that, contrary to common unit testing practice,
 * the order of the tests is important.
 * For example: tests performing reads and writes must be between a
 * test that opens the database and a test that closes it.
 * This means that if one test fails, subsequent tests are more likely to
 * fail as well.
 * Also, tests are listed in increasing complexity, such that later tests
 * can rely on functionality in previous tests to be correct, but are not
 * affected by bugs that can trigger later tests.
 */


///////////////////////////////////////////////////////////////////////////////

static dagdb_key key0 = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0,0,0,0,0,0,0,0,0x37,0xe7,0x52,0x0f};
static int nibbles[] = {
	1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	7,3,7,14,2,5,15,0
};

static void test_nibble() {
	int i;
	for (i=0; i<DAGDB_KEY_LENGTH; i++)
		EX_ASSERT_EQUAL_INT(nibble(key0,i), nibbles[i]);
}

static CU_TestInfo test_non_io[] = {
  { "nibble", test_nibble },
  CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

// The keys are 1 byte too short, as the last byte is filled by the 0-termination character.
static dagdb_key key1 = {"0123456789012345678"};
static dagdb_key key2 = {"0123056789012345678"};
static dagdb_key key3 = {"0123456789012345670"};
static dagdb_key key4 = {"1123456789012345670"};

static void test_data() {
	const char *data = "This is a test";
	uint_fast32_t len = strlen(data);
	dagdb_pointer p = dagdb_data_create(len, data);
	EX_ASSERT_EQUAL_INT(dagdb_get_pointer_type(p), DAGDB_TYPE_DATA);
	EX_ASSERT_EQUAL_INT(dagdb_data_length(p), len);
	CU_ASSERT_NSTRING_EQUAL((const char *) dagdb_data_access(p), data, len);
	dagdb_data_delete(p);
}

static void test_element() {
	dagdb_pointer el = dagdb_element_create(key1, 1000, 1337);
	CU_ASSERT(el);
	EX_ASSERT_EQUAL_INT(dagdb_get_pointer_type(el), DAGDB_TYPE_ELEMENT);
	EX_ASSERT_EQUAL_INT(dagdb_element_data(el), 1000u);
	EX_ASSERT_EQUAL_INT(dagdb_element_backref(el), 1337u);
	key k = obtain_key(el);
	CU_ASSERT_NSTRING_EQUAL(k,key1,DAGDB_KEY_LENGTH);
	dagdb_element_delete(el);
}

static void test_kvpair() {
	// Depends on element
	dagdb_pointer el = dagdb_element_create(key1, 1, 2);
	EX_ASSERT_EQUAL_INT(dagdb_get_pointer_type(el), DAGDB_TYPE_ELEMENT);
	dagdb_pointer kv = dagdb_kvpair_create(el, 42);
	CU_ASSERT(kv);
	EX_ASSERT_EQUAL_INT(dagdb_get_pointer_type(kv), DAGDB_TYPE_KVPAIR);
	EX_ASSERT_EQUAL_INT(dagdb_kvpair_key(kv), el);
	EX_ASSERT_EQUAL_INT(dagdb_kvpair_value(kv), 42u);
	key k = obtain_key(kv);
	CU_ASSERT_NSTRING_EQUAL(k,key1,DAGDB_KEY_LENGTH);
	dagdb_kvpair_delete(kv);
	EX_ASSERT_EQUAL_INT(dagdb_element_data(el), 1u);
	EX_ASSERT_EQUAL_INT(dagdb_element_backref(el), 2u);
	dagdb_element_delete(el);
}

static void test_trie() {
	dagdb_pointer t = dagdb_trie_create();
	CU_ASSERT(t);
	EX_ASSERT_EQUAL_INT(dagdb_get_pointer_type(t), DAGDB_TYPE_TRIE);
	dagdb_trie_delete(t);
}

static CU_TestInfo test_basic_io[] = {
  { "data", test_data },
  { "element", test_element },
  { "kvpair", test_kvpair },
  { "trie", test_trie },
  { "verify_chunk_table", verify_chunk_table },
  CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

static void test_insert() {
	int r;
	dagdb_pointer el1 = dagdb_element_create(key1, 1, 2);
	dagdb_pointer el2 = dagdb_element_create(key2, 3, 4);
	dagdb_pointer el3 = dagdb_element_create(key1, 5, 6); // this is indeed the first key.
	EX_ASSERT_EQUAL_INT(dagdb_get_pointer_type(el1), DAGDB_TYPE_ELEMENT);
	EX_ASSERT_EQUAL_INT(dagdb_get_pointer_type(el2), DAGDB_TYPE_ELEMENT);
	EX_ASSERT_EQUAL_INT(dagdb_get_pointer_type(el3), DAGDB_TYPE_ELEMENT);
	r = dagdb_trie_insert(dagdb_root(), el1);
	EX_ASSERT_EQUAL_INT(r, 1);
	r = dagdb_trie_insert(dagdb_root(), el2);
	EX_ASSERT_EQUAL_INT(r, 1);
	r = dagdb_trie_insert(dagdb_root(), el3); // checking duplicate inserts.
	EX_ASSERT_EQUAL_INT(r, 0);
}

static void test_find() {
	dagdb_pointer el1 = dagdb_trie_find(dagdb_root(), key1);
	dagdb_pointer el2 = dagdb_trie_find(dagdb_root(), key2);
	EX_ASSERT_EQUAL_INT(dagdb_get_pointer_type(el1), DAGDB_TYPE_ELEMENT);
	EX_ASSERT_EQUAL_INT(dagdb_get_pointer_type(el2), DAGDB_TYPE_ELEMENT);
	EX_ASSERT_EQUAL_INT(dagdb_element_data(el1), 1u);
	EX_ASSERT_EQUAL_INT(dagdb_element_backref(el1), 2u);
	EX_ASSERT_EQUAL_INT(dagdb_element_data(el2), 3u);
	EX_ASSERT_EQUAL_INT(dagdb_element_backref(el2), 4u);
	EX_ASSERT_EQUAL_INT(dagdb_trie_find(dagdb_root(), key3), 0u); // fail on empty spot
	EX_ASSERT_EQUAL_INT(dagdb_trie_find(dagdb_root(), key4), 0u); // fail on key mismatch.
}

static void test_remove() {
	// The previous tests should left the database with key1 and key2 
	// in the root trie and nothing else in there.
	EX_ASSERT_EQUAL_INT(dagdb_trie_remove(dagdb_root(), key3), 0); // fail on empty spot
	EX_ASSERT_EQUAL_INT(dagdb_trie_remove(dagdb_root(), key4), 0); // fail on key mismatch.
	EX_ASSERT_EQUAL_INT(dagdb_trie_remove(dagdb_root(), key1), 1); // Remove existing key
	EX_ASSERT_EQUAL_INT(dagdb_trie_remove(dagdb_root(), key1), 0); // fail on already removed key

	dagdb_pointer el1 = dagdb_trie_find(dagdb_root(), key1); // Cannot find removed key.
	EX_ASSERT_EQUAL_INT(el1, 0u);

	dagdb_pointer el2 = dagdb_trie_find(dagdb_root(), key2); // Can properly find other key.
	EX_ASSERT_EQUAL_INT(dagdb_get_pointer_type(el2), DAGDB_TYPE_ELEMENT);
	EX_ASSERT_EQUAL_INT(dagdb_element_data(el2), 3u);
	EX_ASSERT_EQUAL_INT(dagdb_element_backref(el2), 4u);

	EX_ASSERT_EQUAL_INT(dagdb_trie_remove(dagdb_root(), key2), 1); // Removed other existing key
	el2 = dagdb_trie_find(dagdb_root(), key2);
	EX_ASSERT_EQUAL_INT(el2, 0u);
}

static void test_trie_kvpair() {
	dagdb_pointer el = dagdb_element_create(key1, 1, 2);
	EX_ASSERT_EQUAL_INT(dagdb_get_pointer_type(el), DAGDB_TYPE_ELEMENT);
	dagdb_pointer kv = dagdb_kvpair_create(el, 3);
	EX_ASSERT_EQUAL_INT(dagdb_get_pointer_type(kv), DAGDB_TYPE_KVPAIR);
	
	EX_ASSERT_EQUAL_INT(dagdb_trie_insert(dagdb_root(), kv), 1); // Insert kv-pair using key1
	EX_ASSERT_EQUAL_INT(dagdb_trie_insert(dagdb_root(), el), 0); // inserting element using key1 fails
	EX_ASSERT_EQUAL_INT(dagdb_trie_find(dagdb_root(), key1), kv); // The key value pair can be found

	EX_ASSERT_EQUAL_INT(dagdb_trie_remove(dagdb_root(), key1), 1); // We can remove an element that uses key1
	EX_ASSERT_EQUAL_INT(dagdb_kvpair_key(kv), el); // This wont affect the key value pair. 
	EX_ASSERT_EQUAL_INT(dagdb_kvpair_value(kv), 3u);
	dagdb_element_delete(el);
}
 
static void test_trie_recursive_delete() {
	dagdb_pointer t = dagdb_trie_create();
	int r1 = dagdb_trie_insert(t, dagdb_element_create(key1, 0, 2));
	int r2 = dagdb_trie_insert(t, dagdb_element_create(key1, 1, 2));
	int r3 = dagdb_trie_insert(t, dagdb_element_create(key2, 1, 2));
	int r4 = dagdb_trie_insert(t, dagdb_element_create(key3, 1, 2));
	int r5 = dagdb_trie_insert(t, dagdb_element_create(key4, 1, 2));
	CU_ASSERT(r1 && r2==0 && r3 && r4 && r5);
	verify_chunk_table();
	dagdb_trie_delete(t);
	// TODO: (high) Somehow check that the trie has been released
	verify_chunk_table();
}

static CU_TestInfo test_trie_io[] = {
  { "insert", test_insert },
  { "find", test_find },
  { "remove", test_remove },
  { "kvpair", test_trie_kvpair },
  { "recursive_delete", test_trie_recursive_delete },
  { "verify_chunk_table", verify_chunk_table },
  CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

CU_SuiteInfo base_suites[] = {
	{ "base-non-io",   NULL,        NULL,     test_non_io },
	{ "base-basic-io", open_new_db, close_db, test_basic_io },
	{ "base-trie-io",  open_new_db, close_db, test_trie_io },
	CU_SUITE_INFO_NULL,
};
