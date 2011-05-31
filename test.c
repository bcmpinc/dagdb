#include <stdio.h>
#include "base.h"

#define NEW_TEST(x) { printf("Testing %s: ", #x); int failed=0;
#define CHECK(a, ...) if (a) printf("."); else { printf("x"); if (!failed) {failed=1; printf(__VA_ARGS__);}}
#define END_TEST if (failed) printf(" failed.\n"); else printf(" succeeded.\n"); }
#define NEW_GROUP(x) { printf("\n --- %s ---\n", #x); 
#define SKIP_GROUP(x) if (0) {
#define END_GROUP }

const char * hex = "0123456789abcdef";

int main(char * argv, int argc) {
	printf(" === Testing DAGDB ===\n");
	
	NEW_TEST(nibble)
		// Test that hash nibbles are extracted correctly.
		int i;
		dagdb_hash h = {0x01234567,0x89abcdef,0x00000000,0x3c3c3c3c,0x2222dddd};
		// printf("%p, %p, %x\n", h[0], h[0]>>4, h[0]&0xf);
		char * t = "0123456789abcdef000000003c3c3c3c2222dddd";
		for (i=0; i<40; i++) {
			int v = t[i];
			int w = dagdb_nibble(h, i);
			CHECK(w>=0 && w<16 && hex[w] == v, " [%c != %c] ", hex[w], v);
		}
	END_TEST
	
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
		END_TEST
	END_GROUP
	
	return 0;
}
