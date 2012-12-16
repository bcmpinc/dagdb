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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "base.h"
#include "error.h"
#include "mem.h"

///////////////////
// Pointer types //
///////////////////

STATIC_ASSERT(DAGDB_TYPE_DATA    < S,pointer_size_too_small_to_contain_type_data);
STATIC_ASSERT(DAGDB_TYPE_ELEMENT < S,pointer_size_too_small_to_contain_type_element);
STATIC_ASSERT(DAGDB_TYPE_TRIE    < S,pointer_size_too_small_to_contain_type_trie);
STATIC_ASSERT(DAGDB_TYPE_KVPAIR  < S,pointer_size_too_small_to_contain_type_kvpair);
/**
 * Obtains the type inormation of the given pointer.
 */
inline dagdb_pointer_type dagdb_get_pointer_type(dagdb_pointer location) {
	return location&DAGDB_TYPE_MASK;
}


////////////////
// Data items //
////////////////

/**
 * Data item.
 * S bytes: length
 * length bytes rounded up to a multiple of S: data
 */
typedef struct {
	dagdb_size length;
	char data[0];
} Data;
STATIC_ASSERT(sizeof(Data)==S,invalid_data_size);

/**
 * Allocates a data chunk of specified length.
 * The provided data is copied into the chunk.
 * If memory allocation succeeds, a pointer to the data chunk is returned.
 * Otherwise, this function returns 0.
 */
dagdb_pointer dagdb_data_create(dagdb_size length, const void *data) {
	dagdb_pointer r = dagdb_malloc(sizeof(Data) + length);
	if (!r) return 0;
	Data* d = LOCATE(Data,r);
	d->length = length;
	memcpy(d->data, data, length);
	return r | DAGDB_TYPE_DATA;
}

void dagdb_data_delete(dagdb_pointer location) {
	assert(dagdb_get_pointer_type(location) == DAGDB_TYPE_DATA);
	Data* d = LOCATE(Data,location);
	dagdb_free(location, sizeof(Data) + d->length);
}

dagdb_size dagdb_data_length(dagdb_pointer location) {
	assert(dagdb_get_pointer_type(location) == DAGDB_TYPE_DATA);
	Data* d = LOCATE(Data,location);
	return d->length;
}

const void *dagdb_data_access(dagdb_pointer location) {
	assert(dagdb_get_pointer_type(location) == DAGDB_TYPE_DATA);
	Data* d = LOCATE(Data,location);
	return d->data;
}


//////////////
// Elements //
//////////////

/**
 * Element
 * DAGDB_KEY_LENGTH bytes: key
 * 4 bytes: padding. (can be used to store information about this element.
 * S bytes: forward pointer (data or trie)
 * S bytes: pointer to backref. (trie)
 * TODO (low): for small data elements, embed data in this element.
 */
typedef struct {
	unsigned char key[DAGDB_KEY_LENGTH];
	uint32_t dummy;
	dagdb_pointer backref;
	dagdb_pointer data;
} Element;

/**
 * Allocates an element with specified key.
 * The data and backref pointers are copied into the element.
 * If memory allocation succeeds, a pointer to the element is returned.
 * Otherwise, this function returns 0.
 */
dagdb_pointer dagdb_element_create(dagdb_key key, dagdb_pointer data, dagdb_pointer backref) {
	dagdb_pointer r = dagdb_malloc(sizeof(Element));
	if (!r) return 0;
	Element*  e = LOCATE(Element, r);
	memcpy(e->key, key, DAGDB_KEY_LENGTH);
	e->data = data;
	e->backref = backref;
	return r | DAGDB_TYPE_ELEMENT;
}

void dagdb_element_delete(dagdb_pointer location) {
	assert(dagdb_get_pointer_type(location) == DAGDB_TYPE_ELEMENT);
	dagdb_free(location, sizeof(Element));
}

dagdb_pointer dagdb_element_data(dagdb_pointer location) {
	assert(dagdb_get_pointer_type(location) == DAGDB_TYPE_ELEMENT);
	Element*  e = LOCATE(Element, location);
	return e->data;
}

dagdb_pointer dagdb_element_backref(dagdb_pointer location) {
	assert(dagdb_get_pointer_type(location) == DAGDB_TYPE_ELEMENT);
	Element*  e = LOCATE(Element, location);
	return e->backref;
}


//////////////
// KV Pairs //
//////////////

/**
 * KV Pair
 * S bytes: Key (element)
 * S bytes: Value (element or trie)
 * TODO (med): Remove this (and embed them in the tries)
 */
typedef struct  {
	dagdb_pointer key;
	dagdb_pointer value;
} KVPair;

/**
 * Returns 0 if memory allocation fails.
 */
dagdb_pointer dagdb_kvpair_create(dagdb_pointer key, dagdb_pointer value) {
	assert(dagdb_get_pointer_type(key) == DAGDB_TYPE_ELEMENT);
	dagdb_pointer r = dagdb_malloc(sizeof(KVPair));
	if (!r) return 0;
	KVPair*  p = LOCATE(KVPair, r);
	p->key = key;
	p->value = value;
	return r | DAGDB_TYPE_KVPAIR;
}

void dagdb_kvpair_delete(dagdb_pointer location) {
	assert(dagdb_get_pointer_type(location) == DAGDB_TYPE_KVPAIR);
	dagdb_free(location, 2 * S);
}

dagdb_pointer dagdb_kvpair_key(dagdb_pointer location) {
	assert(dagdb_get_pointer_type(location) == DAGDB_TYPE_KVPAIR);
	KVPair*  p = LOCATE(KVPair, location);
	return p->key;
}

dagdb_pointer dagdb_kvpair_value(dagdb_pointer location) {
	assert(dagdb_get_pointer_type(location) == DAGDB_TYPE_KVPAIR);
	KVPair*  p = LOCATE(KVPair, location);
	return p->value;
}


///////////
// Tries //
///////////

/**
 * To allow us to store a pointer to a key.
 */
typedef uint8_t * key;

/** 
 * The trie
 * 16 * S bytes: Pointers (trie or element)
 * 
 * The elements of a map are stored in a special kind of tree, known as a trie.
 * Leave nodes store the keys and their associated values. (element or kvpair)
 * Parent nodes store up to 2^4 = 16 pointers to child nodes.
 * 
 * Let x be a key that is stored below node n. 
 * Let k denote the level of the node. For the root node k=0, for its child nodes k=1.
 * Then the k-th 4 bits of key x denotes the index of the pointer to the child node that stores x.
 * 
 * TODO (low): make tries much more space efficient. 
 *             Instead of 16, support 256 child nodes. With the help of a bitmap, 
 *             we can limit the storage requirement to only the non-null pointers.
 *             This alternative requires realloc to be implemented.
 * TODO (low): embed kv-pairs into tries.
 *             The current implementation stores a single pointer to a pair of pointers.
 *             Embedding the two pointers into the trie would prevent one IO and reduce memory usage of kvpairs by one third.
 * TODO (low): make tries implicit (with a single pointer to its location) to allow realloc operations.
 * 
 */
typedef struct {
	dagdb_pointer entry[16];
} Trie;

/**
 * Returns 0 if memory allocation fails.
 */
dagdb_pointer dagdb_trie_create()
{
	dagdb_pointer r = dagdb_malloc(sizeof(Trie));
	if (!r) return 0;
	return r | DAGDB_TYPE_TRIE;
}

/**
 * Recursively removes this try.
 */
void dagdb_trie_delete(dagdb_pointer location)
{
	assert(dagdb_get_pointer_type(location) == DAGDB_TYPE_TRIE);
	uint_fast32_t i;
	Trie*  t = LOCATE(Trie, location);
	for (i=0; i<16; i++) {
		if (dagdb_get_pointer_type(t->entry[i]) == DAGDB_TYPE_TRIE)
			dagdb_trie_delete(t->entry[i]);
	}
	dagdb_free(location, sizeof(Trie));
}

static uint_fast32_t nibble(const uint8_t * key, uint_fast32_t index) {
	assert(index >= 0);
	assert(index < 2*DAGDB_KEY_LENGTH);
	if (index&1)
		return (key[index>>1]>>4)&0xf;
	else
		return key[index>>1]&0xf;
}

/**
 * Retrieves the key from an Element or from the key part of the KVPair.
 */
static key obtain_key(dagdb_pointer pointer) {
	// Obtain the key.
	if (dagdb_get_pointer_type(pointer) == DAGDB_TYPE_KVPAIR) {
		assert(pointer>=HEADER_SIZE);
		KVPair* kv = LOCATE(KVPair,pointer);
		pointer = kv->key;
	}
	assert(pointer>=HEADER_SIZE);
	assert(dagdb_get_pointer_type(pointer) == DAGDB_TYPE_ELEMENT);
	Element* e = LOCATE(Element,pointer);
	return e->key;
}

/**
 * Retrieves the pointer associated with the given key.
 * If no value is associated, then 0 is returned.
 */
dagdb_pointer dagdb_trie_find(dagdb_pointer trie, dagdb_key k)
{
	assert(trie>=HEADER_SIZE);
	assert(dagdb_get_pointer_type(trie) == DAGDB_TYPE_TRIE);
	
	// Traverse the trie.
	int_fast32_t i;
	for(i=0;i<2*DAGDB_KEY_LENGTH;i++) {
		Trie*  t = LOCATE(Trie, trie);
		int_fast32_t n = nibble(k, i);
		if (t->entry[n]==0) { 
			// Spot is empty, so return null pointer
			return 0;
		}
		if (dagdb_get_pointer_type(t->entry[n]) == DAGDB_TYPE_TRIE) {
			// Descend into the already existing sub-trie
			trie = t->entry[n];
		} else {
			// Check if the element here has the same key.
			key l = obtain_key(t->entry[n]);
			int_fast32_t same = memcmp(k,l,DAGDB_KEY_LENGTH);
			if (same == 0) return t->entry[n];
			
			// The key's differ, so the requested key is not in this trie.
			return 0;
		}
	}
	UNREACHABLE;
}

/**
 * Inserts the given pointer into the trie.
 * The pointer must be an Element or KVPair.
 * Returns 1 if the element is indeed added to the trie.
 * Returns 0 if the element was already in the trie.
 * Returns -1 if the element is not in the trie and an error occured
 * while trying to add it.
 */
int dagdb_trie_insert(dagdb_pointer trie, dagdb_pointer pointer)
{
	assert(trie>=HEADER_SIZE);
	assert(pointer>=HEADER_SIZE);
	assert(dagdb_get_pointer_type(trie) == DAGDB_TYPE_TRIE);
	
	key k = obtain_key(pointer);
	
	// Traverse the trie.
	int_fast32_t i;
	for(i=0;i<2*DAGDB_KEY_LENGTH;i++) {
		Trie*  t = LOCATE(Trie, trie);
		int_fast32_t n = nibble(k, i);
		if (t->entry[n]==0) { 
			// Spot is empty, so we can insert it here.
			t->entry[n] = pointer;
			return 1;
		}
		if (dagdb_get_pointer_type(t->entry[n]) == DAGDB_TYPE_TRIE) {
			// Descend into the already existing sub-trie
			trie = t->entry[n];
		} else {
			// Check if the element here has the same key.
			key l = obtain_key(t->entry[n]);
			int_fast32_t same = memcmp(k,l,DAGDB_KEY_LENGTH);
			if (same == 0) return 0;
			
			// Create new tries until we have a differing nibble
			int_fast32_t m = nibble(l,i);
			while (n == m) {
				dagdb_pointer newtrie = dagdb_trie_create();
				if (!newtrie) {
					// An error occured. Use dagdb_last_error() to obtain the reason.
					return -1;
				}
				Trie*  t2 = LOCATE(Trie, newtrie);
				i++;
				
				// Push the existing element's pointer into the new trie.
				m = nibble(l,i);
				t2->entry[m] = t->entry[n];
				t->entry[n] = newtrie;
				n = nibble(k,i);
				t = t2;
			}
			t->entry[n] = pointer;
		}
	}
	UNREACHABLE;
}

/**
 * Erases the value associated with the given key in this trie.
 * If no value is associated, then this function will do nothing.
 * Returns 1 if the key-value pair is erased from the trie, and 0 otherwise.
 */
int dagdb_trie_remove(dagdb_pointer trie, dagdb_key k)
{
	assert(trie>=HEADER_SIZE);
	assert(dagdb_get_pointer_type(trie) == DAGDB_TYPE_TRIE);
	
	// Traverse the trie.
	int_fast32_t i;
	for(i=0;i<2*DAGDB_KEY_LENGTH;i++) {
		Trie*  t = LOCATE(Trie, trie);
		int_fast32_t n = nibble(k, i);
		if (t->entry[n]==0) { 
			// Spot is empty, so return null pointer
			return 0;
		}
		if (dagdb_get_pointer_type(t->entry[n]) == DAGDB_TYPE_TRIE) {
			// Descend into the already existing sub-trie
			trie = t->entry[n];
		} else {
			// Check if the element here has the same key.
			key l = obtain_key(t->entry[n]);
			int_fast32_t same = memcmp(k,l,DAGDB_KEY_LENGTH);
			if (same == 0) {
				t->entry[n] = 0;
				return 1;
			}
			
			// The key's differ, so the requested key is not in this trie.
			return 0;
		}
	}
	UNREACHABLE;
}

dagdb_pointer dagdb_root()
{
	Header*  h = LOCATE(Header, 0);
	// Lazily create root trie.
	if (h->root==0) {
		// Create the root trie.
		h->root=dagdb_trie_create();
	}
	assert(h->root>=HEADER_SIZE);
	return h->root;
}

