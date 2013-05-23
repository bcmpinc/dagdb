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

#include <gcrypt.h>
#include <assert.h>

#include "api.h"
#include "base.h"

/** @file
 * Contains the implementation of most of the public api functions.
 * 
 * There are two types:
 * - a handle, used to referece an element or a set in an elements backref;
 * - an iterator, used to traverse through a record, a map (backref) or a set (backref entry).
 * 
 * The iterator implementation is found in iterators.c.
 * 
 * There is no way to iterate over the root trie that stores all elements.
 * The find and write methods should already be sufficient to search the root trie.
 * 
 * The function dagdb_select should be used to obtain the value of a specific field 
 * in a record. This method can also be used on a backref to find a specific set.
 * 
 * There is a method that returns a pointer to the backref of an element. 
 */

/** 
 * @struct dagdb_record_entry 
 * Holds a single entry of a record. 
 * @var dagdb_record_entry::key 
 * Stores the fieldname of the record entry.
 * @var dagdb_record_entry::value 
 * Stores the value of the given field.
 */

typedef uint8_t dagdb_hash[DAGDB_KEY_LENGTH];
static void dagdb_data_hash(dagdb_hash h, int length, const void * data) {
	gcry_md_hash_buffer(GCRY_MD_SHA1, h, data, length);
}

static int cmphash(const void *p1, const void *p2) {
    return memcmp(p1, p2, DAGDB_KEY_LENGTH);
}

/** Applies bitwise inversion to the hash, to avoid hash collisions between data and records. */
static void flip_hash(dagdb_hash h) {
	for (long i=0; i<DAGDB_KEY_LENGTH; i++) {
		h[i]=~h[i];
	}
}

static void dagdb_record_hash(dagdb_hash h, long int entries, dagdb_record_entry * items) {
	dagdb_hash * keylist = malloc(entries * sizeof(dagdb_key) * 2);
	for (long i=0; i<entries*2; i++) {
		dagdb_element_key(keylist[i], ((dagdb_pointer*)items)[i]);
	}
	qsort(keylist, entries, DAGDB_KEY_LENGTH*2, cmphash);
	dagdb_data_hash(h, entries * 2 * DAGDB_KEY_LENGTH, keylist);
	flip_hash(h);
}

/**
 * Obtains a reference to the element storing the given byte array. 
 * Returns 0 if the element is not found.
 * @see dagdb_write_bytes
 */
dagdb_handle dagdb_find_bytes(uint64_t length, const char* data) {
	dagdb_hash h;
	dagdb_data_hash(h, length, data);
	return dagdb_trie_find(dagdb_root(), h);
}

/**
 * Obtains a reference to the element storing the given record.
 * Returns 0 if the record is not in the database.
 * @see dagdb_write_record
 */
dagdb_handle dagdb_find_record(uint_fast32_t entries, dagdb_record_entry * items) {
	dagdb_hash h;
	dagdb_record_hash(h, entries, items);
	return dagdb_trie_find(dagdb_root(), h);
}

/**
 * Returns a reference to an element that stores the given byte array.
 * The element is created if it does not yet exist in the database.
 * Returns 0 in case of an error.
 * @see dagdb_find_bytes
 */
dagdb_handle dagdb_write_bytes(uint64_t length, const char* data) {
	dagdb_hash h;
	dagdb_data_hash(h, length, data);
	
	// Check if it already exists.
	dagdb_handle r = dagdb_trie_find(dagdb_root(), h);
	if (r) return r;
	
	dagdb_handle dataptr = 0;
	dagdb_handle backref = 0;
	dagdb_handle element = 0;
	
	// Create data, backref and element.
	dataptr = dagdb_data_create(length, data);
	if (!dataptr) goto error;
	backref = dagdb_trie_create();
	if (!backref) goto error;
	element = dagdb_element_create(h, dataptr, backref);
	if (!element) goto error;
	int res = dagdb_trie_insert(dagdb_root(), element);
	if (res<0) goto error;

	// Inserted in root trie, return handle.
	return element;
	
	error:
	if (element) dagdb_element_delete(element);
	if (backref) dagdb_trie_delete(backref);
	if (dataptr) dagdb_data_delete(dataptr);
	return 0;
}

/**
 * Returns a reference to an element that stores the given record.
 * The element is created if it does not yet exist in the database.
 * Returns 0 in case of an error.
 * 
 * If the element is created, then this method also creates entries in 
 * the backref of the values stored in the record.
 * In the backref of value, the key is searched and an empty trie is created
 * for key if necessary. Then the created record is added into that trie.
 * 
 * @see dagdb_find_bytes
 */
dagdb_handle dagdb_write_record(uint_fast32_t entries, dagdb_record_entry* items) {
	// Compute the hash of the entry.
	dagdb_hash h;
	dagdb_record_hash(h, entries, items);
	
	// Check if it already exists.
	dagdb_handle r = dagdb_trie_find(dagdb_root(), h);
	if (r) return r;
	
	dagdb_handle record = 0;
	dagdb_handle backref = 0;
	dagdb_handle element = 0;

	// Create the trie, backref and element
	record = dagdb_trie_create();
	if (!record) goto error;
	backref = dagdb_trie_create();
	if (!backref) goto error;
	element = dagdb_element_create(h, record, backref);
	if (!element) goto error;
	int res = dagdb_trie_insert(dagdb_root(), element);
	if (res<0) goto error;

	for (uint_fast32_t i=0; i<entries; i++) {
		// TODO: properly handle failures in here.
		int res;
		
		// Obtain the backref trie of the i-th element being refered
		dagdb_handle i_backref = dagdb_element_backref(items[i].value);
		assert(i_backref > 0);
		
		// Obtain the hash of the i-th key
		dagdb_hash key_hash;
		dagdb_element_key(key_hash, items[i].key);
		
		// In the backref get the trie for our key (create if non-existant)
		dagdb_handle i_kv = dagdb_trie_find(i_backref, key_hash);
		dagdb_handle i_keytrie;
		if (i_kv>0) {
			i_keytrie = dagdb_kvpair_value(i_kv);
		} else {
			i_keytrie = dagdb_trie_create();
			i_kv = dagdb_kvpair_create(items[i].key, i_keytrie);
			res = dagdb_trie_insert(i_backref, i_kv);
			assert(res==1);
		}
		
		// Insert a reference to our new record.
		res = dagdb_trie_insert(i_keytrie, element);
		assert(res==1);
		
		// Insert in our record trie
		dagdb_handle kv = dagdb_kvpair_create(items[i].key, items[i].value);
		res = dagdb_trie_insert(record, kv);
		assert(res==1);
	}

	// Inserted in root trie, return handle.
	return element;

	error:
	if (element) dagdb_element_delete(element);
	if (backref) dagdb_trie_delete(backref);
	if (record) dagdb_trie_delete(record);
	return 0;
}

dagdb_handle_type dagdb_get_handle_type(dagdb_handle item) {
	switch(dagdb_get_pointer_type(item)) {
		case DAGDB_TYPE_ELEMENT:
			switch(dagdb_get_pointer_type(dagdb_element_data(item))) {
				case DAGDB_TYPE_DATA:
					return DAGDB_HANDLE_BYTES;
				case DAGDB_TYPE_TRIE:
					return DAGDB_HANDLE_RECORD;
				default:
					return DAGDB_HANDLE_INVALID;
			}
		case DAGDB_TYPE_TRIE:
			return DAGDB_HANDLE_MAP;
		default:
			return DAGDB_HANDLE_INVALID;
	}
}

uint64_t dagdb_bytes_length(dagdb_handle h) {
	if (dagdb_get_pointer_type(h)!=DAGDB_TYPE_ELEMENT) return 0;
	dagdb_pointer data = dagdb_element_data(h);
	if (dagdb_get_pointer_type(data)!=DAGDB_TYPE_DATA) return 0;
	return dagdb_data_length(data);
}

/** Reads bytes from a bytes handle into the given buffer.
 * Will read at most max_size bytes, starting at the position given by offset.
 * If there are less than max_bytes, this will read until the end of the bytes element.
 * Bytes in the buffer after the number of read bytes will remain untouched.
 * @return number of bytes read.
 */
uint64_t dagdb_bytes_read(uint8_t* buffer, dagdb_handle h, uint64_t offset, uint64_t max_size) {
	if (dagdb_get_pointer_type(h)!=DAGDB_TYPE_ELEMENT) return 0;
	dagdb_pointer data = dagdb_element_data(h);
	if (dagdb_get_pointer_type(data)!=DAGDB_TYPE_DATA) return 0;
	uint64_t length = dagdb_data_length(data);
	if (offset > length) return 0;
	if (offset + max_size > length) 
		max_size = length - offset;
	memcpy(buffer, dagdb_data_access(data), max_size);
	return max_size;
}

/** Returns a handle to the backref of given element. */
dagdb_handle dagdb_back_reference(dagdb_handle element) {
	if (dagdb_get_pointer_type(element)!=DAGDB_TYPE_ELEMENT) return 0;
	dagdb_pointer backref = dagdb_element_backref(element);
	return backref;
}

/** Returns a handle to the value of a specific field in a record or to a specific set in a backref. 
 * Returns 0 in case nothing was found or an error occured.
 */
dagdb_handle dagdb_select(dagdb_handle map, dagdb_handle key) {
	if (dagdb_get_pointer_type(key)!=DAGDB_TYPE_ELEMENT) return 0;
	if (dagdb_get_pointer_type(map)==DAGDB_TYPE_ELEMENT) {
		map = dagdb_element_data(map);
	}
	if (dagdb_get_pointer_type(map)!=DAGDB_TYPE_TRIE) return 0;
	dagdb_hash hash;
	dagdb_element_key(hash, key);
	dagdb_pointer kvpair = dagdb_trie_find(map, hash);
	if (!kvpair) return 0;
	return dagdb_kvpair_value(kvpair);
}


