/*
    DagDB - A lightweight structured database system.
    Copyright (C) 2012  B.J. Conijn <bcmpinc@users.sourceforge.net>

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

// include the entire file being tested.
#include "../src/bitarray.c"

#include "test.h"

static void test_array_size() {
	EX_ASSERT_EQUAL_INT(DAGDB_BITARRAY_ARRAY_SIZE(1), 1);
	EX_ASSERT_EQUAL_INT(DAGDB_BITARRAY_ARRAY_SIZE(B-1), 1);
	EX_ASSERT_EQUAL_INT(DAGDB_BITARRAY_ARRAY_SIZE(B), 1);
	EX_ASSERT_EQUAL_INT(DAGDB_BITARRAY_ARRAY_SIZE(B+1), 2);
	EX_ASSERT_EQUAL_INT(DAGDB_BITARRAY_ARRAY_SIZE(B*41+1), 42);
	EX_ASSERT_EQUAL_INT(DAGDB_BITARRAY_ARRAY_SIZE(B*42-1), 42);
	EX_ASSERT_EQUAL_INT(DAGDB_BITARRAY_ARRAY_SIZE(B*42), 42);
	EX_ASSERT_EQUAL_INT(DAGDB_BITARRAY_ARRAY_SIZE(B*42+1), 43);
}

/*
void         dagdb_bitarray_mark  (dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length);
void         dagdb_bitarray_unmark(dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length);
void         dagdb_bitarray_flip  (dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length);
int_fast32_t dagdb_bitarray_check (dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length, int_fast32_t value);
int_fast32_t dagdb_bitarray_read  (dagdb_bitarray* bitarray, int_fast32_t pos);
*/

static void test_mark() {
	const int N = 32;
	dagdb_bitarray b[N];
	memset(b,0,sizeof(b));
	int i;
	for(i=0; i<N; i++)
		EX_ASSERT_EQUAL_LONG_HEX(b[i], 0);
	// marking small parts.
	dagdb_bitarray_mark(b,12,8);
	EX_ASSERT_EQUAL_LONG_HEX(b[0], 0xff000UL);
	dagdb_bitarray_mark(b,16,8);
	EX_ASSERT_EQUAL_LONG_HEX(b[0], 0xfff000UL);
	dagdb_bitarray_mark(b,32,8);
	EX_ASSERT_EQUAL_LONG_HEX(b[0], 0xff00fff000UL);
	
	// marking B bits
	dagdb_bitarray_mark(b,B,B);
	EX_ASSERT_EQUAL_LONG_HEX(b[0], 0xff00fff000UL);
	EX_ASSERT_EQUAL_LONG_HEX(b[1], ~0UL);
	EX_ASSERT_EQUAL_LONG_HEX(b[2], 0UL);
	dagdb_bitarray_mark(b,B*5/2,B);
	EX_ASSERT_EQUAL_LONG_HEX(b[2], 0xffffffff00000000UL);
	EX_ASSERT_EQUAL_LONG_HEX(b[3], 0x00000000ffffffffUL);

	// marking slightly more than B bits
	dagdb_bitarray_mark(b,B*9/2+3,B+2);
	EX_ASSERT_EQUAL_LONG_HEX(b[4], 0xfffffff800000000UL);
	EX_ASSERT_EQUAL_LONG_HEX(b[5], 0x0000001fffffffffUL);

	// marking 3*B bits (aligned)
	dagdb_bitarray_mark(b,B*7,B*3);
	EX_ASSERT_EQUAL_LONG_HEX(b[6], 0UL);
	EX_ASSERT_EQUAL_LONG_HEX(b[7], ~0UL);
	EX_ASSERT_EQUAL_LONG_HEX(b[8], ~0UL);
	EX_ASSERT_EQUAL_LONG_HEX(b[9], ~0UL);
	EX_ASSERT_EQUAL_LONG_HEX(b[10], 0UL);

	// marking much more than B bits (unaligned)
	dagdb_bitarray_mark(b,B*21/2-3,5*B+21);
	EX_ASSERT_EQUAL_LONG_HEX(b[10], 0xffffffffe0000000UL);
	EX_ASSERT_EQUAL_LONG_HEX(b[11], ~0UL);
	EX_ASSERT_EQUAL_LONG_HEX(b[12], ~0UL);
	EX_ASSERT_EQUAL_LONG_HEX(b[13], ~0UL);
	EX_ASSERT_EQUAL_LONG_HEX(b[14], ~0UL);
	EX_ASSERT_EQUAL_LONG_HEX(b[15], 0x0003ffffffffffffUL);
}

static void test_unmark() {
	dagdb_bitarray b[1];
	b[0]=~0;

	// unmarking small parts.
	dagdb_bitarray_unmark(b,12,8);
	EX_ASSERT_EQUAL_LONG_HEX(b[0], ~0xff000UL);
	dagdb_bitarray_unmark(b,16,8);
	EX_ASSERT_EQUAL_LONG_HEX(b[0], ~0xfff000UL);
	dagdb_bitarray_unmark(b,32,8);
	EX_ASSERT_EQUAL_LONG_HEX(b[0], ~0xff00fff000UL);
}

static void test_flip() {
	dagdb_bitarray b[1];
	b[0]=0;

	// flipping small parts.
	dagdb_bitarray_flip(b,12,8);
	EX_ASSERT_EQUAL_LONG_HEX(b[0], 0xff000UL);
	dagdb_bitarray_flip(b,16,8);
	EX_ASSERT_EQUAL_LONG_HEX(b[0], 0xf0f000UL);
	dagdb_bitarray_flip(b,32,8);
	EX_ASSERT_EQUAL_LONG_HEX(b[0], 0xff00f0f000UL);
	dagdb_bitarray_flip(b,0,B);
	EX_ASSERT_EQUAL_LONG_HEX(b[0], ~0xff00f0f000UL);
	dagdb_bitarray_flip(b,1,14);
	EX_ASSERT_EQUAL_LONG_HEX(b[0], ~0xff00f08ffeUL);
}

static void test_read() {
	dagdb_bitarray b[1];
	b[0]=0;
	dagdb_bitarray_mark(b,2,10); // 0000 1111 1111 1100
	dagdb_bitarray_unmark(b,5,3);// 0000 1111 0001 1100
	dagdb_bitarray_flip(b,3,8);  // 0000 1000 1110 0100
	EX_ASSERT_EQUAL_LONG_HEX(b[0], 0x00000000000008e4UL);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0x0)?1:0, 0);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0x1)?1:0, 0);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0x2)?1:0, 1);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0x3)?1:0, 0);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0x4)?1:0, 0);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0x5)?1:0, 1);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0x6)?1:0, 1);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0x7)?1:0, 1);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0x8)?1:0, 0);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0x9)?1:0, 0);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0xa)?1:0, 0);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0xb)?1:0, 1);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0xc)?1:0, 0);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0xd)?1:0, 0);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0xe)?1:0, 0);
	EX_ASSERT_EQUAL_INT(dagdb_bitarray_read(b, 0xf)?1:0, 0);
}

static void test_check() {
	
}

static CU_TestInfo test_bitarrray[] = {
	{ "array_size", test_array_size },
	{ "read", test_read },
	{ "mark", test_mark },
	{ "unmark", test_unmark },
	{ "flip", test_flip },
	{ "check", test_check },
	CU_TEST_INFO_NULL,
};

CU_SuiteInfo bitarray_suites[] = {
	{ "bitarrray",   NULL,        NULL,     test_bitarrray },
	CU_SUITE_INFO_NULL,
};
