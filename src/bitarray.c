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
		bitarray[w1] operator ( (1UL<<b2) - (1UL<<b1) ); \
	} else { \
		bitarray[w1] operator (  0UL      - (1UL<<b1) ); \
		bitarray[w2] operator ( (1UL<<b2) -  1UL      ); \
		for (w1++; w1<w2; w1++) { \
			bitarray[w1] operator (-1UL); \
		} \
	}


/**
 * Set the bits of the given range in a bitarray.
 */
void dagdb_bitarray_mark(dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length) {
	BITARRAY_RANGE_OPERATION(|=);
}

/**
 * Unset the bits of the given range in a bitarray.
 */
void dagdb_bitarray_unmark(dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length) {
	BITARRAY_RANGE_OPERATION(&=~);
}

/**
 * Flip the bits of the given range in a bitarray.
 */
void dagdb_bitarray_flip(dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length) {
	BITARRAY_RANGE_OPERATION(^=);
}
