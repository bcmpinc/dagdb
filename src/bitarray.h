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

#ifndef DAGDB_BITARRAY_H
#define DAGDB_BITARRAY_H
#include <stdint.h>

/**
 * Integer type used to store the bitarray.
 */
typedef uint64_t dagdb_bitarray;

/**
 * Computes the length for the dagdb_bitarray array given the number of bits it has to contain.
 */
#define DAGDB_BITARRAY_ARRAY_SIZE(a) ((a-1)/sizeof(dagdb_bitarray)/8+1)

void         dagdb_bitarray_mark  (dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length);
void         dagdb_bitarray_unmark(dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length);
void         dagdb_bitarray_flip  (dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length);
int_fast32_t dagdb_bitarray_check (dagdb_bitarray* bitarray, int_fast32_t start, int_fast32_t length, int_fast32_t value);
int_fast32_t dagdb_bitarray_read  (dagdb_bitarray* bitarray, int_fast32_t pos);

#endif
