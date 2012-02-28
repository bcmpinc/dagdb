#include <UnitTest++.h>
extern "C" {
#include "base2.h"
}

SUITE(base2) {
	TEST(load) {
		int r = dagdb_load("test.dagdb");
		CHECK_EQUAL(r, false);
	}
	
	TEST(unload) {
		dagdb_unload();
	}
}
