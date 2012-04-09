#include <UnitTest++.h>
extern "C" {
#include "base2.h"
#include <string.h>
}

SUITE(base2) {
	TEST(clean) {
		unlink("test.dagdb");
	}

	TEST(load_init) {
		int r = dagdb_load("test.dagdb");
		CHECK(r==0);
	}

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
		dagdb_pointer kv = dagdb_kvpair_create(27,42);
		CHECK(kv);
		CHECK_EQUAL(DAGDB_TYPE_KVPAIR, dagdb_get_type(kv));
		CHECK_EQUAL(27u, dagdb_kvpair_key(kv));
		CHECK_EQUAL(42u, dagdb_kvpair_value(kv));
		dagdb_kvpair_delete(kv);
	}

	TEST(trie) {
		dagdb_pointer t = dagdb_trie_create();
		CHECK(t);
		CHECK_EQUAL(DAGDB_TYPE_TRIE, dagdb_get_type(t));
		dagdb_trie_delete(t);
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
