/*
    DagDB - A lightweight structured database system.
    Copyright (C) 2011  B.J. Conijn <bcmpinc@users.sourceforge.net>

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

#include <UnitTest++.h>
#include <exception>
#include <stdexcept>
#include <cstring>

#include "dagdb/memory.h"

using namespace dagdb::memory;

SUITE(memory_create_data) {
	TEST(text) {
		// Test that data elements can be created and return the right data.
		const char * c = "this is a test";
		size_t l = strlen(c);
		dagdb::Data d = create_data(c, l);
		
		CHECK_EQUAL(d->length(), l);

		char a[100];
		size_t n = d->read(a, 0, l);
		CHECK_EQUAL(n, l);
		CHECK_ARRAY_EQUAL(a, c, l);
	}

	TEST(binary) {
		// Test that binary data elements can be created and return the right data.
		size_t l = 1024;
		uint8_t c[l];
		for (size_t i=0; i<l; i++) 
			c[i] = l&0xff;
		dagdb::Data d = create_data(c, l);
		
		CHECK_EQUAL(d->length(), l);

		uint8_t a[l];
		size_t n = d->read(a, 0, l);
		CHECK_EQUAL(n, l);
		CHECK_ARRAY_EQUAL((char*)a, (char*)c, l);
	}

	TEST(partial) {
		// Test that partial data can be obtained.
		const char * c = "this is a test";
		size_t l = strlen(c);
		dagdb::Data d = create_data(c, l);
		
		char a[2];
		size_t n = d->read(a, 5, 2);
		CHECK_EQUAL(n, 2u);
		CHECK_ARRAY_EQUAL(a, "is", 2);

		char b[4];
		n = d->read(b, 10, 10);
		CHECK_EQUAL(n, 4u);
		
		CHECK_ARRAY_EQUAL(b, "test", 4);
	}

}

