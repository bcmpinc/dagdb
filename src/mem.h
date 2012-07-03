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

#ifndef DAGDB_MEMORY_H
#define DAGDB_MEMORY_H
#include <stdint.h>

/**
 * The size of a dagdb_pointer or dagdb_size variable.
 * These two are always of equal length.
 * It will also be a power of two.
 * Memory is allocated in multiples of this.
 */
#define S (sizeof(dagdb_size))

/**
 * Masks the part of a pointer that contains type information.
 */
#define DAGDB_TYPE_MASK (S-1)

/**
 * Returns a pointer of type 'type', pointing to the given location in the database file.
 * Also handles stripping off the pointer's type information.
 */
//#define LOCATE(type,location) ((type*)(assert(global.file!=MAP_FAILED),global.file+((location)&~DAGDB_TYPE_MASK)))
#define LOCATE(type,location) ((type*)(dagdb_file+((location)&~DAGDB_TYPE_MASK)))

/**
 * The amount of space (in bytes) reserved for the database header. 
 * Defined in the header, as it is often used in assertions.
 */
#define HEADER_SIZE 512

/**
 * Length of the free memory chunk lists table.
 */
#define CHUNK_TABLE_SIZE 31

typedef uint64_t dagdb_size;
typedef uint64_t dagdb_pointer;

/**
 * Stores some basic information about the database.
 * The size of this header cannot exceed HEADER_SIZE.
 */
typedef struct {
	int32_t magic;
	int32_t format_version;
	dagdb_pointer root;
	dagdb_pointer chunks[2*CHUNK_TABLE_SIZE];
} Header;

extern void* dagdb_file;

dagdb_pointer dagdb_malloc (dagdb_size length);
dagdb_pointer dagdb_realloc(dagdb_pointer location, dagdb_size oldlength, dagdb_size newlength);
void          dagdb_free   (dagdb_pointer location, dagdb_size length);

#endif
