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
#include <assert.h>

#include "bitarray.h"

#define B (sizeof(dagdb_bitarray)*8)

#define BITARRAY_RANGE_OPERATION(operator) \
	assert(start>=0); \
	assert(length>=0); \
	int_fast32_t w1 = start/B, w2 = (start+length)/B; \
	int_fast32_t b1 = start%B, b2 = (start+length)%B; \
	\
	if (w1==w2) { \
		operator(bitarray[w1], ( (1UL<<b2) - (1UL<<b1) )); \
	} else { \
		operator(bitarray[w1], (  0UL      - (1UL<<b1) )); \
		operator(bitarray[w2], ( (1UL<<b2) -  1UL      )); \
		for (w1++; w1<w2; w1++) { \
			operator(bitarray[w1], (-1UL)); \
		} \
	}

/**
 * Set the bits of the given range in a bitarray.
 */
void dagdb_bitarray_mark(dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length) {
#define ORIS(a,b) a |= b
	BITARRAY_RANGE_OPERATION(ORIS);
}

/**
 * Unset the bits of the given range in a bitarray.
 */
void dagdb_bitarray_unmark(dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length) {
#define ANDISINV(a,b) a &= ~b
	BITARRAY_RANGE_OPERATION(ANDISINV);
}

/**
 * Flip the bits of the given range in a bitarray.
 */
void dagdb_bitarray_flip(dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length) {
#define XORIS(a,b) a ^= b
	BITARRAY_RANGE_OPERATION(XORIS);
}

/**
 * Checks whether a bit exists that is set to value in the queried range.
 */
int_fast32_t dagdb_bitarray_check ( dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length, int_fast32_t value ) {
#define CHECK(a,b) if((a & b) != b * value) return 0
	assert(value==0 || value==1);
	BITARRAY_RANGE_OPERATION(CHECK);
	return 1;
}

/**
 * Returns the value of the bit at the given position.
 */
int_fast32_t dagdb_bitarray_read(dagdb_bitarray* bitarray, int_fast32_t pos) {
	return bitarray[pos/(sizeof(dagdb_bitarray)*8)] & (1UL << pos%(sizeof(dagdb_bitarray)*8));
}

