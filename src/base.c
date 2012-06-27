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

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "base.h"

/////////////
// Macro's //
/////////////


/**
 * Used to perform sanity checks during compilation.
 */
#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1]

/**
 * The size of a dagdb_pointer or dagdb_size variable.
 * These two are always of equal length.
 * It will also be a power of two.
 */
#define S (sizeof(dagdb_size))
STATIC_ASSERT(S == sizeof(dagdb_size), same_pointer_size_size);
STATIC_ASSERT(((S - 1)&S) == 0, size_power_of_two);

/**
 * Maximum amount of space (in bytes) that the database is allowed to use.
 * TODO (low): Remove this limit.
 */
#define MAX_SIZE (1<<30)

/**
 * Masks the part of a pointer that contains type information.
 */
#define DAGDB_TYPE_MASK (S-1)

/**
 * Returns a pointer of type 'type', pointing to the given location in the database file.
 * Also handles stripping off the pointer's type information.
 */
#define LOCATE(type,location) (type*)(file+(location&~DAGDB_TYPE_MASK))

/**
 * The amount of space (in bytes) reserved for the database header. 
 */
#define HEADER_SIZE 128

/**
 * Counter for the database format. Incremented whenever a format change
 * is incompatible with previous versions of this library.
 */
#define FORMAT_VERSION 1

/**
 * A 4 byte string that helps identifying a DagDB database.
 * It can also be used to identify the used byte order.
 */
#define DAGDB_MAGIC (*(uint32_t*)"D-db")

//////////////////////
// Static variables //
//////////////////////

/**
 * The file descriptor of the currently opened database.
 */
static uint_fast32_t database_fd;

/**
 * Pointer to the data of the file in memory.
 */
static void *file;

/**
 * The current size of the currently opened database.
 */
static dagdb_size size;

///////////
// Types //
///////////

/**
 * Stores some basic information about the database.
 * The size of this header cannot exceed HEADER_SIZE.
 */
typedef struct {
	int32_t magic;
	int32_t format_version;
	dagdb_pointer root;
} Header;
STATIC_ASSERT(sizeof(Header) <= HEADER_SIZE, header_too_large);

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
inline dagdb_pointer_type dagdb_get_type(dagdb_pointer location) {
	return location&DAGDB_TYPE_MASK;
}

//////////////////////
// Space allocation //
//////////////////////

/** The size of a single memory page. This value can differ between systems, but is usually 4096. */
#define PAGE_SIZE 4096

/** Size of a single memory slab/chunk. */
#define CHUNK_SIZE (8 * PAGE_SIZE)

/** Number of ints that fit in the slab (in ints), while perserving room for the use bitmap. */
#define BITMAP_SIZE ((CHUNK_SIZE+S*S) / (S*S+1))

/**
 * This is embedded in all free memory chunks,
 * except those that are too small.
 * These create linked lists of chunks that are approximately the same size.
 */
typedef struct {
	dagdb_pointer prev; 
	dagdb_pointer next;
} FreeMemoryChunk;
STATIC_ASSERT(sizeof(FreeMemoryChunk)/S*S == sizeof(FreeMemoryChunk), fmc_size_is_multiple_of_s);

/**
 * Computes the log2 of an 32-bit integer.
 * Based on: http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog.
 */
static inline uint_fast32_t lg2(register uint_fast32_t v) {
	register uint_fast32_t r; // result of log2(v) will go here
	register uint_fast32_t shift;
	r =     (v > 0xFFFF) << 4; v >>= r;
	shift = (v > 0xFF  ) << 3; v >>= shift; r |= shift;
	shift = (v > 0xF   ) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3   ) << 1; v >>= shift; r |= shift;
	r |= (v >> 1);
	return r;
}

/**
 * Rounds up the given argument to an allocatable size.
 * This is either the smallest allocatable size or a multiple of S.
 */
dagdb_size dagdb_round_up(dagdb_size v) {
	if (v<sizeof(FreeMemoryChunk)) return sizeof(FreeMemoryChunk);
	return  -(~(sizeof(dagdb_pointer) - 1) & -v);
}

/**
 * Allocates the requested amount of bytes.
 * Currently always enlarges the file by 'length'.
 */
static dagdb_size dagdb_malloc(dagdb_size length) {
	dagdb_pointer r = size;
	dagdb_size alength = dagdb_round_up(length);
	size += alength;
	assert(size <= MAX_SIZE);
	if (ftruncate(database_fd, size)) {
		perror("dagdb_malloc");
		abort();
	}
	assert((r&DAGDB_TYPE_MASK) == 0);
	return r;
}

/**
 * Enlarges or shrinks the provided memory such that its size is the given amount of bytes.
 * If the current data must be moved to a different location, this function will perform that move.
 * Note that the new allocated memory might be at a different location (even if shrinking).
 * TODO (high): unimplemented / implement realloc
 */
static dagdb_size dagdb_realloc(dagdb_pointer location, dagdb_size oldlength, dagdb_size newlength) {
	return 0;
}

/**
 * Frees the provided memory.
 * Currently only zero's out the memory range.
 * This function also strips off the type information before freeing.
 * TODO (high): add to memory pool & truncate file if possible.
 */
static void dagdb_free(dagdb_pointer location, dagdb_size length) {
	// Strip type information
	location &= ~DAGDB_TYPE_MASK;
	// Do sanity checks
	assert(location>=HEADER_SIZE);
	assert(location+length<=size);
	// Clear memory
	memset(file + location, 0, dagdb_round_up(length));
}

//////////////////////
// Database loading //
//////////////////////

/**
 * Opens the given file. Creates it if it does not yet exist.
 * @returns 0 if succesfull.
 * TODO (high): give more information in case of error.
 * TODO (high): convert failing assertions to load failure.
 */
int dagdb_load(const char *database) {
	// Open the database file
	int_fast32_t fd = open(database, O_RDWR | O_CREAT, 0644);
	if (fd == -1) goto error;
	//printf("Database file opened %d\n", fd);

	// Map database into memory
	file = mmap(NULL, MAX_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (file == MAP_FAILED) goto error;
	//printf("Database data @ %p\n", file);

	// Obtain length of database file
	size = lseek(fd, 0, SEEK_END);
	//printf("Database size: %d\n", size);
	
	// Check or generate header information.
	Header* h = LOCATE(Header,0);
	if (size < HEADER_SIZE) {
		database_fd = fd;
		// Database must be empty.
		assert(size==0);
		if (ftruncate(fd, HEADER_SIZE)) {
			perror("dagdb_load init");
			abort();
		}
		h->magic=DAGDB_MAGIC;
		h->format_version=FORMAT_VERSION;
		size = HEADER_SIZE;
		h->root=dagdb_trie_create();
	} else {
		// database size must be a multiple of S
		assert((size&DAGDB_TYPE_MASK)==0); 
		// check headers.
		assert(h->magic==DAGDB_MAGIC);
		assert(h->format_version==FORMAT_VERSION);
		//printf("Database format %d accepted.\n", h->format_version);
		database_fd = fd;
	}

	//printf("Opened DB\n");
	return 0;

error:
	fflush(stdout);
	perror("dagdb_load");
	if (file != MAP_FAILED) file = 0;
	if (fd != -1) close(fd);
	return -1;
}

/**
 * Closes the current database file.
 * Does nothing if no database file is currently open.
 */
void dagdb_unload() {
	if (file) {
		assert(file!=MAP_FAILED);
		munmap(file, MAX_SIZE);
		file = 0;
		//printf("Unmapped DB\n");
	}
	if (database_fd) {
		close(database_fd);
		database_fd = 0;
		//printf("Closed DB\n");
	}
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

dagdb_pointer dagdb_data_create(dagdb_size length, const void *data) {
	dagdb_pointer r = dagdb_malloc(sizeof(Data) + length);
	Data* d = LOCATE(Data,r);
	d->length = length;
	memcpy(d->data, data, length);
	return r | DAGDB_TYPE_DATA;
}

void dagdb_data_delete(dagdb_pointer location) {
	assert(dagdb_get_type(location) == DAGDB_TYPE_DATA);
	Data* d = LOCATE(Data,location);
	dagdb_free(location, sizeof(Data) + d->length);
}

dagdb_size dagdb_data_length(dagdb_pointer location) {
	assert(dagdb_get_type(location) == DAGDB_TYPE_DATA);
	Data* d = LOCATE(Data,location);
	return d->length;
}

const void *dagdb_data_read(dagdb_pointer location) {
	assert(dagdb_get_type(location) == DAGDB_TYPE_DATA);
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

dagdb_pointer dagdb_element_create(dagdb_key key, dagdb_pointer data, dagdb_pointer backref) {
	dagdb_pointer r = dagdb_malloc(sizeof(Element));
	Element*  e = LOCATE(Element, r);
	memcpy(e->key, key, DAGDB_KEY_LENGTH);
	e->data = data;
	e->backref = backref;
	return r | DAGDB_TYPE_ELEMENT;
}

void dagdb_element_delete(dagdb_pointer location) {
	assert(dagdb_get_type(location) == DAGDB_TYPE_ELEMENT);
	dagdb_free(location, sizeof(Element));
}

dagdb_pointer dagdb_element_data(dagdb_pointer location) {
	assert(dagdb_get_type(location) == DAGDB_TYPE_ELEMENT);
	Element*  e = LOCATE(Element, location);
	return e->data;
}

dagdb_pointer dagdb_element_backref(dagdb_pointer location) {
	assert(dagdb_get_type(location) == DAGDB_TYPE_ELEMENT);
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

dagdb_pointer dagdb_kvpair_create(dagdb_pointer key, dagdb_pointer value) {
	assert(dagdb_get_type(key) == DAGDB_TYPE_ELEMENT);
	dagdb_pointer r = dagdb_malloc(sizeof(KVPair));
	KVPair*  p = LOCATE(KVPair, r);
	p->key = key;
	p->value = value;
	return r | DAGDB_TYPE_KVPAIR;
}

void dagdb_kvpair_delete(dagdb_pointer location) {
	assert(dagdb_get_type(location) == DAGDB_TYPE_KVPAIR);
	dagdb_free(location, 2 * S);
}

dagdb_pointer dagdb_kvpair_key(dagdb_pointer location) {
	assert(dagdb_get_type(location) == DAGDB_TYPE_KVPAIR);
	KVPair*  p = LOCATE(KVPair, location);
	return p->key;
}

dagdb_pointer dagdb_kvpair_value(dagdb_pointer location) {
	assert(dagdb_get_type(location) == DAGDB_TYPE_KVPAIR);
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
 * TODO (med): make tries much more space efficient.
 * TODO (med): embed kv-pairs into tries.
 * TODO (low): make tries implicit (with a single pointer to its location) to allow realloc operations.
 */
typedef struct {
	dagdb_pointer entry[16];
} Trie;

dagdb_pointer dagdb_trie_create()
{
	return dagdb_malloc(sizeof(Trie)) | DAGDB_TYPE_TRIE;
}

/**
 * Recursively removes this try.
 */
void dagdb_trie_delete(dagdb_pointer location)
{
	assert(dagdb_get_type(location) == DAGDB_TYPE_TRIE);
	uint_fast32_t i;
	Trie*  t = LOCATE(Trie, location);
	for (i=0; i<16; i++) {
		if (dagdb_get_type(t->entry[i]) == DAGDB_TYPE_TRIE)
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
	if (dagdb_get_type(pointer) == DAGDB_TYPE_KVPAIR) {
		assert(pointer>=HEADER_SIZE);
		KVPair* kv = LOCATE(KVPair,pointer);
		pointer = kv->key;
	}
	assert(pointer>=HEADER_SIZE);
	assert(dagdb_get_type(pointer) == DAGDB_TYPE_ELEMENT);
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
	assert(dagdb_get_type(trie) == DAGDB_TYPE_TRIE);
	
	// Traverse the trie.
	int_fast32_t i;
	for(i=0;i<2*DAGDB_KEY_LENGTH;i++) {
		Trie*  t = LOCATE(Trie, trie);
		int_fast32_t n = nibble(k, i);
		if (t->entry[n]==0) { 
			// Spot is empty, so return null pointer
			return 0;
		}
		if (dagdb_get_type(t->entry[n]) == DAGDB_TYPE_TRIE) {
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
	fprintf(stderr,"Unreachable state reached in '%s'\n", __func__);
	abort();
}

/**
 * Inserts the given pointer into the trie.
 * The pointer must be an Element or KVPair.
 * Returns 1 if the element is indeed added to the trie.
 * Returns 0 if the element was already in the trie.
 */
int dagdb_trie_insert(dagdb_pointer trie, dagdb_pointer pointer)
{
	assert(trie>=HEADER_SIZE);
	assert(dagdb_get_type(trie) == DAGDB_TYPE_TRIE);
	
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
		if (dagdb_get_type(t->entry[n]) == DAGDB_TYPE_TRIE) {
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
	fprintf(stderr,"Unreachable state reached in '%s'\n", __func__);
	abort();
}

/**
 * Erases the value associated with the given key in this trie.
 * If no value is associated, then this function will do nothing.
 * Returns 1 if the key-value pair is erased from the trie, and 0 otherwise.
 */
int dagdb_trie_remove(dagdb_pointer trie, dagdb_key k)
{
	assert(trie>=HEADER_SIZE);
	assert(dagdb_get_type(trie) == DAGDB_TYPE_TRIE);
	
	// Traverse the trie.
	int_fast32_t i;
	for(i=0;i<2*DAGDB_KEY_LENGTH;i++) {
		Trie*  t = LOCATE(Trie, trie);
		int_fast32_t n = nibble(k, i);
		if (t->entry[n]==0) { 
			// Spot is empty, so return null pointer
			return 0;
		}
		if (dagdb_get_type(t->entry[n]) == DAGDB_TYPE_TRIE) {
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
	fprintf(stderr,"Unreachable state reached in '%s'\n", __func__);
	abort();
}

dagdb_pointer dagdb_root()
{
	Header*  h = LOCATE(Header, 0);
	assert(h->root>=HEADER_SIZE);
	return h->root;
}
