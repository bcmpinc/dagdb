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
#include <errno.h>

#include "base.h"


/////////////
// Macro's //
/////////////

/**
 * Used to perform sanity checks during compilation.
 */
#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1]

/**
 * Set a new error message.
 */
#define dagdb_report(fmt, ...) snprintf(global.errormsg, sizeof(global.errormsg), "%s:" fmt ".", __func__, ##__VA_ARGS__);

/**
 * Set a new error message using the description provided by the standard library.
 */
#define dagdb_report_p(fmt, ...) do { \
	int_fast32_t l = snprintf(global.errormsg, sizeof(global.errormsg), "%s:" fmt ":", __func__, ##__VA_ARGS__); \
	strerror_r(errno, global.errormsg + l, sizeof(global.errormsg) - l); \
} while(0)

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
#define LOCATE(type,location) ((type*)(assert(global.file!=MAP_FAILED),global.file+(location&~DAGDB_TYPE_MASK)))

/**
 * The amount of space (in bytes) reserved for the database header. 
 */
#define HEADER_SIZE 512

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

/**
 * Length of the free memory chunk lists table.
 */
#define CHUNK_TABLE_SIZE 31

/**
 * Minimal allocatable chunk size (in bytes).
 * This is just sufficient to store the next and previous pointer of the linked list.
 */
#define MIN_CHUNK_SIZE 2*S

/**
 * Maximum allocatable chunk size (in bytes).
 * This is 'hand-picked' and based on CHUNK_TABLE_SIZE.
 * There is a test 'test_chunk_id' that computes the correct value for this #define and verifies that it equals the value below.
 */
#define MAX_CHUNK_SIZE 766*S

///////////
// Types //
///////////

/**
 * This is embedded in all free memory chunks, except those that are too small.
 * These create linked lists of chunks that are approximately the same size.
 * 
 * Parameters only exist when sufficient space is available. The first two parameters always exist, as 
 * defined by MIN_CHUNK_SIZE.
 */
typedef struct {
	dagdb_pointer prev; 
	dagdb_pointer next;
	dagdb_size size;
} FreeMemoryChunk;
STATIC_ASSERT(MIN_CHUNK_SIZE%S == 0, min_chunk_size_is_multiple_of_s);
STATIC_ASSERT(sizeof(FreeMemoryChunk)%S == 0, fmc_size_is_multiple_of_s);

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
STATIC_ASSERT(sizeof(Header) <= HEADER_SIZE, header_too_large);


//////////////////////
// Static variables //
//////////////////////

static struct {
	/**
	 * The file descriptor of the currently opened database.
	 */
	uint_fast32_t database_fd;

	/**
	 * Pointer to the data of the file in memory.
	 */
	void *file;

	/**
	 * The current size of the currently opened database.
	 */
	dagdb_size size;

	/**
	 * Buffer to contain error message.
	 */
	char errormsg[256];
} global = {0, MAP_FAILED, 0, "dagdb: No error."};

dagdb_error_code dagdb_errno;


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

/** Number of bits in a byte. */
#define BITS_PER_BYTE 8

/** The size of a single memory page. This value can differ between systems, but is usually 4096. */
#define PAGE_SIZE 4096

/** Size of a single memory slab. */
#define SLAB_SIZE (8 * 4096)

/** 
 * Number of ints that fit in the slab (in ints), while perserving room for the usage bitmap. 
 */
#define BITMAP_SIZE ((SLAB_SIZE*BITS_PER_BYTE) / (S*BITS_PER_BYTE+1))

/**
 * Returns a dagdb_pointer, pointing to the root element of the linked list with the given id in the free chunk table.
 */
#define CHUNK_TABLE_LOCATION(id) ((dagdb_pointer)&(((Header*)0)->chunks[2*(id)]))

/**
 * Integer type used to store the bitmap.
 */
typedef uint64_t bitmap_base_type;

/**
 * A struct for the internal structure of a slab.
 */
typedef struct {
	dagdb_pointer data[BITMAP_SIZE];
	bitmap_base_type bitmap[(BITMAP_SIZE+sizeof(bitmap_base_type)*8-1)/sizeof(bitmap_base_type)/8];
} MemorySlab;
STATIC_ASSERT(sizeof(MemorySlab) <= SLAB_SIZE, memory_slab_fits_in_a_slab);
STATIC_ASSERT(sizeof(MemorySlab) > SLAB_SIZE - 2*S, memory_slab_does_not_waste_too_much);
#define SLAB_USEABLE_SPACE_SIZE sizeof(((MemorySlab*)0)->data)

/**
 * Rounds up the given argument to an allocatable size.
 * This is either the smallest allocatable size or a multiple of S.
 */
dagdb_size dagdb_round_up(dagdb_size v) {
	if (v<MIN_CHUNK_SIZE) return MIN_CHUNK_SIZE;
	return  -(~(sizeof(dagdb_pointer) - 1) & -v);
}

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
 * Computes to what chunk list a chunk of size v must be added.
 */
static int_fast32_t free_chunk_id(uint_fast32_t v) {
	v/=S;
	v+=4-MIN_CHUNK_SIZE/sizeof(dagdb_pointer);
	if (v<4) return -1;
	uint_fast32_t l = lg2(v);
	l=((l<<2) | ((v>>(l-2)) & 3))-8;
	if (l>=CHUNK_TABLE_SIZE) return CHUNK_TABLE_SIZE-1;
	return l;
}

/**
 * Tells what chunk list we must use to find a chunk of at least size v.
 * The argument v is not allowed to exceed MAX_CHUNK_SIZE.
 */
static int_fast32_t alloc_chunk_id(uint_fast32_t v) {
	assert(v<=MAX_CHUNK_SIZE); // must have been checked before calling.
	return free_chunk_id(v-1)+1;
}

/**
 * Inserts the given chunk into the free chunk linked list table.
 */
static void dagdb_chunk_insert(dagdb_pointer location, dagdb_size size) {
	assert(size>=MIN_CHUNK_SIZE);
	assert(location%S==0);
	assert(location>=HEADER_SIZE);
	assert(location%SLAB_SIZE + size <= SLAB_USEABLE_SPACE_SIZE);
	int_fast32_t id = free_chunk_id(size);
	assert(id>=0 && id<CHUNK_TABLE_SIZE);
	dagdb_pointer table = CHUNK_TABLE_LOCATION(id);
	FreeMemoryChunk * n = LOCATE(FreeMemoryChunk, location);
	FreeMemoryChunk * t = LOCATE(FreeMemoryChunk, table);
	n->prev = table;
	n->next = t->next;
	t->next = location;
	if (size>=sizeof(FreeMemoryChunk)) {
		n->size = size;
	}
}

/**
 * Removes the given chunk from its linked list.
 */
static void dagdb_chunk_remove(dagdb_pointer location) {
	FreeMemoryChunk * c = LOCATE(FreeMemoryChunk, location);
	assert(c->next>0);
	assert(c->prev>0);
	LOCATE(FreeMemoryChunk,c->next)->prev = c->prev;
	LOCATE(FreeMemoryChunk,c->prev)->next = c->next;
	c->prev = c->next = 0;
}

/**
 * Sets or unsets bits in a given slab's bitmap.
 */
static inline void dagdb_bitmap_mask_apply(MemorySlab * s, int_fast32_t i, bitmap_base_type m, int_fast32_t value) { 
	if (value) { 
		assert((s->bitmap[i]&m)==0); 
		s->bitmap[i] |= m;
	} else { 
		assert((s->bitmap[i]&m)==m); 
		s->bitmap[i] &= ~m;
	} 
}

#define B (sizeof(bitmap_base_type)*8)
/**
 * Set or unset the usage flag of the given range in a slab's bitmap.
 */
static void dagdb_bitmap_mark(dagdb_pointer location, dagdb_size size, int_fast32_t value) {
	assert(value==0 || value==1);
	assert(location%S==0);
	assert(location%SLAB_SIZE + size <= SLAB_USEABLE_SPACE_SIZE);
	int_fast32_t offset = location & (SLAB_SIZE-1);
	dagdb_pointer base = location-offset;
	MemorySlab * s = LOCATE(MemorySlab, base);
	int_fast32_t len = dagdb_round_up(size)/S;
	offset /= S;
	// dagdb_bitmap_mask_apply(s, location in array, mask end - mask start, value)
	// all bits: 0 - 1 
	if (offset%B+len < B) {
		dagdb_bitmap_mask_apply(s, offset/B, (1UL<<(offset%B+len))-(1UL<<offset%B), value);
	} else {
		dagdb_bitmap_mask_apply(s, offset/B, 0UL-(1UL<<offset%B), value);
		if ((offset+len)%B>0) 
			dagdb_bitmap_mask_apply(s, (offset+len)/B, (1UL<<(offset+len)%B)-1UL, value);
		int_fast32_t i;
		for (i=offset/B+1; i<(offset+len)/B; i++) {
			dagdb_bitmap_mask_apply(s, i, -1, value);
		}
	}
	
}
STATIC_ASSERT((SLAB_SIZE & (SLAB_SIZE-1)) == 0, slab_size_power_of_two);

/**
 * Allocates the requested amount of bytes.
 * Currently always enlarges the file by 'length'.
 * @return A pointer to the newly allocated memory, 0 in case of an error.
 * TODO (high): let all uses of this function check the return value.
 */
static dagdb_pointer dagdb_malloc(dagdb_size length) {
	// Traverse the free chunk table, to find an empty spot in one of the already existing slabs.
	if (length > MAX_CHUNK_SIZE) {
		dagdb_errno = DAGDB_ERROR_BAD_ARGUMENT;
		dagdb_report("Cannot allocate %lub, which is larger than the maximum %lub.", length, MAX_CHUNK_SIZE);
		return 0;
	}
	
	int_fast32_t id = alloc_chunk_id(length);
	FreeMemoryChunk * m = LOCATE(FreeMemoryChunk, CHUNK_TABLE_LOCATION(id));
	while (m->next == m->prev && ++id < CHUNK_TABLE_SIZE) {
		m = LOCATE(FreeMemoryChunk, CHUNK_TABLE_LOCATION(id));
	}
	
	dagdb_pointer r;
	length = dagdb_round_up(length);
	if (id < CHUNK_TABLE_SIZE) {
		// There is a sufficiently large chunk available.
		r = m->next;
		dagdb_chunk_remove(r);
		m = LOCATE(FreeMemoryChunk, r);
		assert(free_chunk_id(m->size)==id);
		if (id > 0 && m->size-length>=MIN_CHUNK_SIZE) {
			dagdb_chunk_insert(r+length, m->size-length);
		}
	} else {
		// Allocate the memory in a newly created slab.
		r = global.size;
		assert((global.size % SLAB_SIZE) == 0);
		dagdb_size new_size = global.size + SLAB_SIZE;
		if (new_size > MAX_SIZE) {
			dagdb_errno = DAGDB_ERROR_DB_TOO_LARGE;
			dagdb_report("Cannot enlarge database of %lub with %ub beyond hard coded limit of %u bytes", global.size, SLAB_SIZE, MAX_SIZE);
			return 0;
		}
		if (ftruncate(global.database_fd, new_size)) {
			dagdb_errno = DAGDB_ERROR_DB_TOO_LARGE;
			dagdb_report_p("Failed to grow database file to %lub", new_size);
			return 0;
		}
		global.size = new_size;
		
		// Insert the unused part of the slab in the free chunk table.
		dagdb_chunk_insert(r+length, SLAB_USEABLE_SPACE_SIZE-length);
	}
	assert((r % S) == 0);
	// Mark the bitmap.
	dagdb_bitmap_mark(r,length,1);
	return r;
}
STATIC_ASSERT(MAX_CHUNK_SIZE % S == 0, chunk_size_multiple_of_S);

/**
 * Enlarges or shrinks the provided memory such that its size is the given amount of bytes.
 * If the current data must be moved to a different location, this function will perform that move.
 * Note that the new allocated memory might be at a different location (even if shrinking).
 * When growing, the newly allocated bits remain uninitialized.
 * TODO (med): unimplemented / implement realloc
 */
static dagdb_pointer dagdb_realloc(dagdb_pointer location, dagdb_size oldlength, dagdb_size newlength) {
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
	assert(location+length<=global.size);
	// Clear memory
	memset(global.file + location, 0, dagdb_round_up(length));
}


//////////////////////
// Database loading //
//////////////////////

/**
 * Fills the header of the database with the necessary default information.
 */
static void dagdb_initialize_header(Header* h) {
	h->magic = DAGDB_MAGIC;
	h->format_version = FORMAT_VERSION;
	
	// Self-link all items in the free chunk table.
	int_fast32_t i;
	for (i=0; i<CHUNK_TABLE_SIZE; i++) {
		FreeMemoryChunk * m = LOCATE(FreeMemoryChunk, CHUNK_TABLE_LOCATION(i));
		m->prev = m->next = CHUNK_TABLE_LOCATION(i);
	}
	
	// Insert the unused part of the slab in the free chunk table.
	dagdb_chunk_insert(HEADER_SIZE, SLAB_USEABLE_SPACE_SIZE-HEADER_SIZE);
	
	// Mark header as used in bitmap
	dagdb_bitmap_mark(0, HEADER_SIZE, 1);
	
	// Create the root trie.
	h->root=dagdb_trie_create();
}

/**
 * Opens the given file. Creates it if it does not yet exist.
 * @returns 0 if successful.
 * TODO (high): give more information in case of error.
 * TODO (high): convert failing assertions to load failure.
 */
int dagdb_load(const char *database) {
	// Open the database file
	int_fast32_t fd = open(database, O_RDWR | O_CREAT, 0644);
	if (fd == -1) {
		dagdb_report_p("Cannot open '%s'", database);
		goto error;
	}

	// Map database into memory
	global.file = mmap(NULL, MAX_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (global.file == MAP_FAILED) {
		dagdb_report_p("Cannot map file to memory");
		goto error;
	}

	// Obtain length of database file
	global.size = lseek(fd, 0, SEEK_END);
	if (global.size>MAX_SIZE) {
		dagdb_report("File exceeds hardcoded size limit %lu > %u", global.size, MAX_SIZE);
		goto error;
	}
	
	// database size must be a multiple of SLAB_SIZE
	if((global.size%SLAB_SIZE)!=0) {
		dagdb_report("File has unexpected size %lu", global.size);
		goto error;
	}
	
	Header* h = LOCATE(Header,0);
	if (global.size == 0) {
		// Database is freshly created, initialize header.
		global.database_fd = fd;
		if (ftruncate(fd, SLAB_SIZE)) {
			dagdb_report_p("Could not allocate %db diskspace for database", SLAB_SIZE);
			goto error;
		}
		global.size = SLAB_SIZE;
		dagdb_initialize_header(h);
	} else {
		// Check headers.
		if(h->magic!=DAGDB_MAGIC) {
			dagdb_report("File has invalid magic");
			goto error;
		}
		if(h->format_version!=FORMAT_VERSION) {
			dagdb_report("File has incompatible format version");
			goto error;
		}
		global.database_fd = fd;
	}

	// Database opened successfully.
	return 0;

error:
	// Something went wrong, so cleanup.
	// At this point the error message must have been set already.
	dagdb_errno = DAGDB_ERROR_INVALID_DB;
	if (global.file!=MAP_FAILED) {
		munmap(global.file, MAX_SIZE);
		global.file = MAP_FAILED;
	}
	if (fd != -1) close(fd);
	return -1;
}

/**
 * Closes the current database file.
 * Does nothing if no database file is currently open.
 */
void dagdb_unload() {
	if (global.file!=MAP_FAILED) {
		munmap(global.file, MAX_SIZE);
		global.file = MAP_FAILED;
		//printf("Unmapped DB\n");
	}
	if (global.database_fd) {
		close(global.database_fd);
		global.database_fd = 0;
		//printf("Closed DB\n");
	}
}

/**
 * Returns a pointer to the static buffer that contains an explanation for the most recent error.
 * If no error has happened, this still returns a pointer to a valid string.
 */
const char* dagdb_last_error() {
	return global.errormsg;
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
