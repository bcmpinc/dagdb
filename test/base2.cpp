#include <UnitTest++.h>
extern "C" {
#include "base2.h"
#include <string.h>
}

SUITE(base2) {
	TEST(load) {
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
		CHECK_EQUAL(len, dagdb_data_length(p));
		CHECK_ARRAY_EQUAL(data, (const char*) dagdb_data_read(p), len);
		dagdb_data_delete(p);
		CHECK_EQUAL(0u, dagdb_data_length(p)); // UNDEFINED BEHAVIOR
	}
	
	TEST(unload) {
		dagdb_unload();
	}
	
	TEST(clean) {
		unlink("test.dagdb");
	}
}
