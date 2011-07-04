#include <UnitTest++.h>
#include <cstdio>
#include <cstring>
#include "base.h"

#define compare_hash(s1, s2) memcmp((s1), (s2), sizeof(dagdb_hash))

const char * hex = "0123456789abcdef";

SUITE(functions) {
	TEST(nibble) {
		// Test that hash nibbles are extracted correctly.
		int i;
		dagdb_hash h;
		// printf("%p, %p, %x\n", h[0], h[0]>>4, h[0]&0xf);
		const char * t = "0123456789abcdef000000003c3c3c3c2222dddd";
		dagdb_parse_hash(h, t);
		for (i=0; i<40; i++) {
			int v = t[i];
			int w = dagdb_nibble(h, i);
			CHECK(w>=0);
			CHECK(w<16);
			CHECK_EQUAL(hex[w],v);
		}
	}
	TEST(hash) {
		dagdb_hash h;
		dagdb_hash hh;
		dagdb_parse_hash(hh, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
		dagdb_get_hash(h, "", 0);
		char calc[41];
		dagdb_write_hash(calc, hh);
		CHECK(compare_hash(h,hh)==0);
	}
}
	
SUITE(pointers) {
	// Test the pointer structure on various assumptions
	int i,j;
	// Types can range between 0 and 255.
	uint64_t types[] = {0,1,4,150,255};
	// Addresses are 56 bits in size
	uint64_t addresses[] = {0, 1234567891, 0x1234567891, 0x00DEADBEAFF0000D, (2ULL<<56)-1};
	
	TEST(type) {
		for (i=0; i<5; i++) {
			for (j=0; j<4; j++) {
				dagdb_pointer p = {types[i],addresses[j]};
				CHECK_EQUAL(p.type, types[i]);
			}
		}
	}
	TEST(address) {
		for (i=0; i<5; i++) {
			for (j=0; j<4; j++) {
				dagdb_pointer p = {types[i],addresses[j]};
				CHECK_EQUAL(p.address, addresses[j]);
			}
		}
	}
}

SUITE(io) {
	TEST(init) {
		// Test that init works at all.
		// Creates files in 'db' directory.
		if (dagdb_init("db")) {
			perror("dagdb_init");
			CHECK(false);
		}
		if (dagdb_truncate()) {
			perror("dagdb_truncate");
			CHECK(false);
		}
	}
	
	const int items = 6;
	uint64_t length[] = {5,5,6,5,11,7};
	const char * data[] = {"Test1","Test2","foobar","12345","\0after-zero","\n\r\t\001\002\003\004"};
	
	TEST(insert) {
		int i;
		// Test that data pieces can be inserted.
		for (i=0; i<items; i++) {
			int r = dagdb_insert_data(data[i], length[i]);
			CHECK_EQUAL(r, 0);
		}
	}
	
	dagdb_pointer p[items];
	TEST(find) {
		int i;
		dagdb_hash h;
		// Test that data pieces can be found.
		for (i=0; i<items; i++) {
			dagdb_get_hash(h, data[i], length[i]);
			int r = dagdb_find(&p[i], h, dagdb_root);
			CHECK_EQUAL(r, 0);
		}
	}
	
	TEST(insert_twice) {
		int i;
		// Test that data pieces are not inserted twice.
		for (i=0; i<items; i++) {
			int r = dagdb_insert_data(data[i], length[i]);
			CHECK_EQUAL(r, 1);
		}
	}

	TEST(type) {
		int i;
		// Test that data pieces have the correct length.
		for (i=0; i<items; i++) {
			CHECK_EQUAL(p[i].type, DAGDB_ELEMENT);
		}
	}

	TEST(length) {
		int i;
		// Test that data pieces have the correct length.
		for (i=0; i<items; i++) {
			dagdb_element e;
			dagdb_read_element(&e, p[i]);
			int64_t l = dagdb_length(e.forward);
			CHECK_EQUAL(l, length[i]);
		}
	}
}
	
int main(int argc, char ** argv) {
	printf("Testing DAGDB\n");
	if (argc>1)
		dagdb_set_log_function(printf);
	return UnitTest::RunAllTests();
}
