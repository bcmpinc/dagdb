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

#ifndef DAGDB_BASE_H
#define DAGDB_BASE_H
#include <stdint.h>

#define DAGDB_KEY_LENGTH 20
typedef const uint8_t dagdb_key[DAGDB_KEY_LENGTH];
typedef uint64_t dagdb_size;
typedef uint64_t dagdb_pointer;

typedef enum {
	DAGDB_TYPE_DATA,
	DAGDB_TYPE_ELEMENT,
	DAGDB_TYPE_TRIE,
	DAGDB_TYPE_KVPAIR,
} dagdb_pointer_type;

// Trie related
dagdb_pointer dagdb_trie_create();
void          dagdb_trie_delete(dagdb_pointer location);
int           dagdb_trie_insert(dagdb_pointer trie, dagdb_pointer pointer);
dagdb_pointer dagdb_trie_find  (dagdb_pointer trie, dagdb_key key);
int           dagdb_trie_remove(dagdb_pointer trie, dagdb_key key);

// Element related
dagdb_pointer dagdb_element_create (dagdb_key key, dagdb_pointer data, dagdb_pointer backref);
void          dagdb_element_delete (dagdb_pointer location);
dagdb_pointer dagdb_element_data   (dagdb_pointer location);
dagdb_pointer dagdb_element_backref(dagdb_pointer location);

// Data related
dagdb_pointer dagdb_data_create(dagdb_size length, const void * data);
void          dagdb_data_delete(dagdb_pointer location);
dagdb_size    dagdb_data_length(dagdb_pointer location);
const void *  dagdb_data_read  (dagdb_pointer location);

// KVpair related
dagdb_pointer dagdb_kvpair_create(dagdb_pointer key,dagdb_pointer value);
void          dagdb_kvpair_delete(dagdb_pointer location);
dagdb_pointer dagdb_kvpair_key   (dagdb_pointer location);
dagdb_pointer dagdb_kvpair_value (dagdb_pointer location);

// Other
int           dagdb_load(const char * database);
void          dagdb_unload();
dagdb_pointer dagdb_root();
dagdb_pointer_type dagdb_get_type(dagdb_pointer location);

#endif 
