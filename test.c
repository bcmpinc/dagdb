#include <stdio.h>
#include "base.h"
#include "string.h"

#define NEW_TEST(x) { printf("Testing %s: ", #x); int failed=0;
#define CHECK(a, ...) if (a) printf("."); else { printf("x"); if (!failed) {failed=1; printf(__VA_ARGS__);}}
#define END_TEST if (failed) printf(" failed.\n"); else printf(" succeeded.\n"); }
#define NEW_GROUP(x) { printf("\n --- %s ---\n", #x); 
#define SKIP_GROUP(x) if (0) {
#define END_GROUP }

#define compare_hash(s1, s2) memcmp((s1), (s2), sizeof(dagdb_hash))

const char * hex = "0123456789abcdef";

int main(int argc, char ** argv) {
	printf(" === Testing DAGDB ===\n");
	if (argc>1)
		dagdb_set_log_function(printf);
	
	NEW_GROUP(functions)
		NEW_TEST(nibble)
			// Test that hash nibbles are extracted correctly.
			int i;
			dagdb_hash h;
			// printf("%p, %p, %x\n", h[0], h[0]>>4, h[0]&0xf);
			char * t = "0123456789abcdef000000003c3c3c3c2222dddd";
			dagdb_parse_hash(h, t);
			for (i=0; i<40; i++) {
				int v = t[i];
				int w = dagdb_nibble(h, i);
				CHECK(w>=0 && w<16 && hex[w] == v, " [%c != %c] ", hex[w], v);
			}
		END_TEST
		NEW_TEST(hash)
			dagdb_hash h;
			dagdb_hash hh;
			dagdb_parse_hash(hh, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
			dagdb_get_hash(h, "", 0);
			char calc[41];
			dagdb_write_hash(calc, hh);
			CHECK(compare_hash(h,hh)==0,"Wrong hash: %s",calc);
		END_TEST
	END_GROUP
	
	NEW_GROUP(pointers)
		// Test the pointer structure on various assumptions
		int i,j;
		// Types can range between 0 and 255.
		int types[] = {0,1,4,150,255};
		// Addresses are 56 bits in size
		uint64_t addresses[] = {0, 1234567891, 0x1234567891, 0x00DEADBEAFF0000D, (2ULL<<56)-1};
		NEW_TEST(type)
			for (i=0; i<5; i++) {
				for (j=0; j<4; j++) {
					dagdb_pointer p = {types[i],addresses[j]};
					CHECK(p.type == types[i], " [%d != %d] ", p.type, types[i]);
				}
			}
		END_TEST
		NEW_TEST(address)
			for (i=0; i<5; i++) {
				for (j=0; j<4; j++) {
					dagdb_pointer p = {types[i],addresses[j]};
					CHECK(p.address == addresses[j], " [%Lu != %Lu] ", (int64_t)p.address, addresses[j]);
				}
			}
		END_TEST
	END_GROUP
	
	NEW_GROUP(io)
		NEW_TEST(init)
			// Test that init works at all.
			// Creates files in 'db' directory.
			if (dagdb_init("db")) {
				perror("dagdb_init");
				failed = 1;
			}
			if (dagdb_truncate()) {
				perror("dagdb_truncate");
				failed = 1;
			}
		END_TEST
		
		int items = 6;
		int length[] = {5,5,6,5,11,7};
		char * data[] = {"Test1","Test2","foobar","12345","\0after-zero","\n\r\t\001\002\003\004"};
		NEW_TEST(insert)
			int i;
			// Test that data pieces can be inserted.
			for (i=0; i<items; i++) {
				int r = dagdb_insert_data(data[i], length[i]);
				CHECK(r==0," [%d!=0] ", r);
			}
		END_TEST
		
		NEW_TEST(find)
			int i;
			dagdb_pointer p;
			dagdb_hash h;
			// Test that data pieces can be found.
			for (i=0; i<items; i++) {
				dagdb_get_hash(h, data[i], length[i]);
				int r = dagdb_find(&p, h, dagdb_root);
				CHECK(r==0," [%d!=0] ", r);
			}
		END_TEST
		
		NEW_TEST(insert_twice)
			int i;
			// Test that data pieces are not inserted twice.
			for (i=0; i<items; i++) {
				int r = dagdb_insert_data(data[i], length[i]);
				CHECK(r==1," [%d!=1] ", r);
			}
		END_TEST
		
	END_GROUP
	
	return 0;
}
