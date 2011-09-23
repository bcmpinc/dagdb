#include <UnitTest++.h>
#include <map>

#include "dagdb.h"


SUITE(dagdb_header) {
	class myit {
	public:
		int key() {return 27;}
		int value() {return 42;}
		void operator++() {}
	};
	class mymap {
	public:
		myit find(int a) {return myit();}
		myit begin() {return myit();}
		myit end() {return myit();}
	};
	
	TEST(types) {
		std::map<int,int> m;
		m[3]=12;
		m[5]=99;
		m[9]=2;
		
		mymap n;
		
		//interface::map a(m);
		//a.begin().key().abs();
		//interface::map b(n);
	}
}

int main(int argc, char **argv) {
	std::printf("Testing DAGDB\n");
	return UnitTest::RunAllTests();
}
