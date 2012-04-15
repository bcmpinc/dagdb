#include <UnitTest++.h>
extern "C" {
#include <string.h>
#include "base2.h"
}

/**
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
SUITE(base2) {
	TEST(round_up) {
		CHECK_EQUAL(0u,dagdb_round_up(0));
		CHECK_EQUAL(4u,dagdb_round_up(1));
		CHECK_EQUAL(4u,dagdb_round_up(2));
		CHECK_EQUAL(4u,dagdb_round_up(3));
		CHECK_EQUAL(4u,dagdb_round_up(4));
		CHECK_EQUAL(256u,dagdb_round_up(255));
		CHECK_EQUAL(256u,dagdb_round_up(256));
		CHECK_EQUAL(260u,dagdb_round_up(257));
		CHECK_EQUAL(260u,dagdb_round_up(258));
		CHECK_EQUAL(260u,dagdb_round_up(259));
		CHECK_EQUAL(260u,dagdb_round_up(260));
	}
	
	TEST(clean) {
		unlink("test.dagdb");
	}

	TEST(load_init) {
		int r = dagdb_load("test.dagdb");
		CHECK(r==0);
	}
	
	TEST(data) {
		const char * data = "This is a test";
		uint len = strlen(data);
		dagdb_pointer p = dagdb_data_create(len, data);
		CHECK_EQUAL(DAGDB_TYPE_DATA, dagdb_get_type(p));
		CHECK_EQUAL(len, dagdb_data_length(p));
		CHECK_ARRAY_EQUAL(data, (const char*) dagdb_data_read(p), len);
		dagdb_data_delete(p);
		CHECK_EQUAL(0u, dagdb_data_length(p)); // UNDEFINED BEHAVIOR
	}
	
	TEST(element) {
		dagdb_pointer el = dagdb_element_create((uint8_t*)"01234567890123456789",1000,1337);
		CHECK(el);
		CHECK_EQUAL(DAGDB_TYPE_ELEMENT, dagdb_get_type(el));
		CHECK_EQUAL(1000u, dagdb_element_data(el));
		CHECK_EQUAL(1337u, dagdb_element_backref(el));
		dagdb_element_delete(el);
	}
	
	TEST(kvpair) {
		dagdb_pointer key = 1000 | DAGDB_TYPE_ELEMENT;
		dagdb_pointer kv = dagdb_kvpair_create(key,42);
		CHECK(kv);
		CHECK_EQUAL(DAGDB_TYPE_KVPAIR, dagdb_get_type(kv));
		CHECK_EQUAL(key, dagdb_kvpair_key(kv));
		CHECK_EQUAL(42u, dagdb_kvpair_value(kv));
		dagdb_kvpair_delete(kv);
	}

	TEST(trie) {
		dagdb_pointer t = dagdb_trie_create();
		CHECK(t);
		CHECK_EQUAL(DAGDB_TYPE_TRIE, dagdb_get_type(t));
		dagdb_trie_delete(t);
	}
	
	// The keys are 1 byte too short, as the last byte is filled by the 0-termination character.
	const uint8_t key1[DAGDB_KEY_LENGTH] = {"0123456789012345678"};
	const uint8_t key2[DAGDB_KEY_LENGTH] = {"0123056789012345678"};
	const uint8_t key3[DAGDB_KEY_LENGTH] = {"0123456789012345670"};
	const uint8_t key4[DAGDB_KEY_LENGTH] = {"1123456789012345670"};
	
	TEST(insertion) {
		int r;
		dagdb_pointer el1 = dagdb_element_create(key1,1,2);
		dagdb_pointer el2 = dagdb_element_create(key2,3,4);
		dagdb_pointer el3 = dagdb_element_create(key1,5,6); // this is indeed the first key.
		CHECK_EQUAL(DAGDB_TYPE_ELEMENT, dagdb_get_type(el1));
		CHECK_EQUAL(DAGDB_TYPE_ELEMENT, dagdb_get_type(el2));
		CHECK_EQUAL(DAGDB_TYPE_ELEMENT, dagdb_get_type(el3));
		r = dagdb_trie_insert(dagdb_root(), el1);
		CHECK_EQUAL(1,r);
		r = dagdb_trie_insert(dagdb_root(), el2);
		CHECK_EQUAL(1,r);
		r = dagdb_trie_insert(dagdb_root(), el3); // checking duplicate inserts.
		CHECK_EQUAL(0,r);
	}
	
	TEST(find) {
		dagdb_pointer el1 = dagdb_trie_find(dagdb_root(), key1);
		dagdb_pointer el2 = dagdb_trie_find(dagdb_root(), key2);
		CHECK_EQUAL(DAGDB_TYPE_ELEMENT, dagdb_get_type(el1));
		CHECK_EQUAL(DAGDB_TYPE_ELEMENT, dagdb_get_type(el2));
		CHECK_EQUAL(1,dagdb_element_data(el1));
		CHECK_EQUAL(2,dagdb_element_backref(el1));
		CHECK_EQUAL(3,dagdb_element_data(el2));
		CHECK_EQUAL(4,dagdb_element_backref(el2));
		CHECK_EQUAL(0,dagdb_trie_find(dagdb_root(), key3)); // fail on empty spot
		CHECK_EQUAL(0,dagdb_trie_find(dagdb_root(), key4)); // fail on key mismatch.
	}
	
	TEST(unload) {
		dagdb_unload();
	}
	
	TEST(load_reload) {
		int r = dagdb_load("test.dagdb");
		CHECK(r==0);
		dagdb_unload();
	}
}
