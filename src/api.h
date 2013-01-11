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

#ifndef DAGDB_API_H
#define DAGDB_API_H
#include <stdint.h>

typedef uint64_t dagdb_handle;

typedef struct {
	dagdb_handle key;
	dagdb_handle value;
} dagdb_record_entry;

typedef struct {
	
} dagdb_iterator;

typedef enum {
	DAGDB_HANDLE_BYTES,
	DAGDB_HANDLE_RECORD,
	DAGDB_HANDLE_MAP,
	DAGDB_HANDLE_INVALID,
} dagdb_handle_type;

int           dagdb_load(const char * database);
void          dagdb_unload();

dagdb_handle  dagdb_write_bytes(uint64_t length, const char * data);
dagdb_handle  dagdb_write_record(uint_fast32_t entries, dagdb_record_entry * items);
dagdb_handle  dagdb_find_bytes(uint64_t length, const char * data);
dagdb_handle  dagdb_find_record(uint_fast32_t entries, dagdb_record_entry * items);

dagdb_handle_type dagdb_get_handle_type(dagdb_handle item);

// Data only methods.
uint64_t          dagdb_bytes_length(dagdb_handle h);
uint64_t          dagdb_bytes_read(uint8_t * buffer, dagdb_handle h, uint64_t offset, uint64_t max_size);

// Record/map/set only methods
dagdb_handle      dagdb_select(dagdb_handle map, dagdb_handle key);
dagdb_iterator *  dagdb_iterator_create(dagdb_handle src);
void              dagdb_iterator_destroy(dagdb_iterator * it);
void              dagdb_iterator_advance(dagdb_iterator * it);
dagdb_handle      dagdb_iterator_key(dagdb_iterator * it);
dagdb_handle      dagdb_iterator_value(dagdb_iterator * it);

#endif