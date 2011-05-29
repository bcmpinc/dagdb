#include <stdio.h>
#include "dagdb.h"

#define NEW_TEST(x) { printf("Testing %s: ", #x); int failed=0;
#define CHECK(a) if (a || (failed=1, printf("x"), 0)) printf("."); else
#define END_TEST if (failed) printf(" failed.\n"); else printf(" succeeded.\n"); }


int main() {
	printf(" === Testing DAGDB ===\n");
	
	NEW_TEST(nibble)
		int i;
		dagdb_hash h = {0x01234567,0x89abcdef,0x00000000,0x3c3c3c3c,0x2222dddd};
		// printf("%p, %p, %x\n", h[0], h[0]>>4, h[0]&0xf);
		char * t = "0123456789abcdef000000003c3c3c3c2222dddd";
		for (i=0; i<40; i++) {
			int v = t[i];
			if (v>='0' && v<='9') v=v-'0';
			if (v>='a' && v<='f') v=v-'a'+10;
			int w = dagdb_nibble(h, i);
			CHECK(w == v);
			// printf(" [%d != %d] ", w, v);
		}
	END_TEST
	
	return 0;
}