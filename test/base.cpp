/*
    DagDB - A lightweight structured database system.
    Copyright (C) 2011  B.J. Conijn <bcmpinc@sourceforge.net>

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

#include "base.h"

using namespace dagdb::storage;

const char *hex = "0123456789abcdef";

SUITE(functions) {
	TEST(nibble) {
		// Test that hash nibbles are extracted correctly.
		// printf("%p, %p, %x\n", h[0], h[0]>>4, h[0]&0xf);
		const char *t = "0123456789abcdef000000003c3c3c3c2222dddd";
		Hash h(t);
		for (int i = 0; i < 40; i++) {
			int v = t[i];
			int w = h.nibble(i);
			CHECK(w >= 0);
			CHECK(w < 16);
			CHECK_EQUAL(hex[w], v);
		}
	}
	TEST(hash) {
		Hash h("", 0);
		Hash hh("da39a3ee5e6b4b0d3255bfef95601890afd80709");
		char calc[41];
		hh.write(calc);
		CHECK(h == hh);
	}
}

SUITE(pointers) {
	// Test the pointer structure on various assumptions
	// Types can range between 0 and 255.
	int types[] = {0, 1, 4, 150, 255};
	// Addresses are 56 bits in size
	uint64_t addresses[] = {0, 1234567891, 0x1234567891, 0x00DEADBEAFF0000D, (2ULL << 56) - 1};

	TEST(type) {
		for (int i = 0; i < 5; i++) {
			for (int j = 0; j < 4; j++) {
				Pointer p(types[i], addresses[j]);
				CHECK_EQUAL(p.type, types[i]);
			}
		}
	}
	TEST(address) {
		for (int i = 0; i < 5; i++) {
			for (int j = 0; j < 4; j++) {
				Pointer p(types[i], addresses[j]);
				CHECK_EQUAL(p.address, addresses[j]);
			}
		}
	}
}

SUITE(io) {
	TEST(init) {
		// Test that init works at all.
		// Creates files in 'db' directory.
		init("db");
		truncate();
	}

	const int items = 6;
	uint64_t length[] = {5, 5, 6, 5, 11, 7};
	const char *data[] = {"Test1", "Test2", "foobar", "12345", "\0after-zero", "\n\r\t\001\002\003\004"};
	TEST(insert) {
		// Test that data pieces can be inserted.
		for (int i = 0; i < items; i++) {
			int r = insert_data(data[i], length[i]);
			CHECK_EQUAL(r, true);
		}
	}

	Pointer p[items];
	TEST(find) {
		// Test that data pieces can be found.
		for (int i = 0; i < items; i++) {
			Hash h(data[i], length[i]);
			p[i] = root.find(h);
			CHECK(p[i] != root);
		}
	}

	TEST(insert_twice) {
		// Test that data pieces are not inserted twice.
		for (int i = 0; i < items; i++) {
			int r = insert_data(data[i], length[i]);
			CHECK_EQUAL(r, false);
		}
	}

	TEST(type) {
		// Test that pointers have the correct type.
		for (int i = 0; i < items; i++) {
			CHECK_EQUAL(p[i].type, Element::info.type);
		}
	}

	TEST(length) {
		// Test that data pieces have the correct length.
		for (int i = 0; i < items; i++) {
			Element e;
			e.read(p[i]);
			uint64_t l = e.forward.data_length();
			CHECK_EQUAL(l, length[i]);
		}
	}

	TEST(content) {
		Data d;
		for (int i = 0; i < items; i++) {
			Element e;
			e.read(p[i]);
			d.read(e.forward);
			CHECK_EQUAL(d.length, length[i]);
			CHECK_ARRAY_EQUAL(d.data, (uint8_t *)data[i], d.length);
		}
	}

	TEST(wrong_data_pointer_read) {
		Data d;
		CHECK_THROW(d.read(root), std::invalid_argument);
		CHECK_THROW(d.read(p[0]), std::invalid_argument);
	}
}


