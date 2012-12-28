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
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "mem.h"
#include "error.h"
#include "bitarray.h"

/////////////
// Macro's //
/////////////

/**
 * Maximum amount of space (in bytes) that the database is allowed to use.
 * TODO (low): Remove this limit.
 */
#define MAX_SIZE (1<<30)

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

STATIC_ASSERT(sizeof(Header) <= HEADER_SIZE, header_too_large);
STATIC_ASSERT(S == sizeof(dagdb_size), same_pointer_size_size);
STATIC_ASSERT(((S - 1)&S) == 0, size_power_of_two);


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


//////////////////////
// Static variables //
//////////////////////

/**
 * The file descriptor of the currently opened database.
 */
static uint_fast32_t dagdb_database_fd;

/**
 * Pointer to the data of the file in memory.
 */
void *dagdb_file = MAP_FAILED;

/**
 * The current size of the currently opened database.
 */
static dagdb_size dagdb_database_size;


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
 * A struct for the internal structure of a slab.
 */
typedef struct {
	dagdb_pointer data[BITMAP_SIZE];
	dagdb_bitarray bitmap[DAGDB_BITARRAY_ARRAY_SIZE(BITMAP_SIZE)];
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
	assert(size<=SLAB_USEABLE_SPACE_SIZE);
	assert(location%S==0);
	assert(location>=HEADER_SIZE);
	assert(location%SLAB_SIZE + size <= SLAB_USEABLE_SPACE_SIZE);
	int_fast32_t id = free_chunk_id(size);
	assert(id>=0 && id<CHUNK_TABLE_SIZE);
	dagdb_pointer table = CHUNK_TABLE_LOCATION(id);
	FreeMemoryChunk * n = LOCATE(FreeMemoryChunk, location);
	FreeMemoryChunk * t = LOCATE(FreeMemoryChunk, table);
	FreeMemoryChunk * o = LOCATE(FreeMemoryChunk, t->next);
	n->prev = table;
	n->next = t->next;
	t->next = location;
	o->prev = location;
	if (size>=sizeof(FreeMemoryChunk)) {
		n->size = size;
		*LOCATE(dagdb_size, location + size - S) = size; // Also write the length of the chunk at the end.
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
 * Set or unset the usage flag of the given range in a slab's bitmap.
 */
static void dagdb_bitmap_mark(dagdb_pointer location, dagdb_size size, int_fast32_t value) {
	assert(value==0 || value==1);
	assert(location%S==0);
	assert(location%SLAB_SIZE + size <= SLAB_USEABLE_SPACE_SIZE);
	int_fast32_t offset = location & (SLAB_SIZE-1);
	MemorySlab * s = LOCATE(MemorySlab, location-offset);
	(value?dagdb_bitarray_mark:dagdb_bitarray_unmark)(s->bitmap, offset/S, dagdb_round_up(size)/S);
}
STATIC_ASSERT((SLAB_SIZE & (SLAB_SIZE-1)) == 0, slab_size_power_of_two);

/**
 * Allocates the requested amount of bytes.
 * WARNING: Like malloc, these are not zero'd out. However, as the chunks tend to be
 * from new diskspace, which is initialized to 0, this might seem to be the case.
 * 
 * @return A pointer to the newly allocated memory, 0 in case of an error.
 */
dagdb_pointer dagdb_malloc(dagdb_size length) {
	// Traverse the free chunk table, to find an empty spot in one of the already existing slabs.
	if (length > MAX_CHUNK_SIZE) {
		dagdb_errno = DAGDB_ERROR_BAD_ARGUMENT;
		dagdb_report("Cannot allocate %lub, which is larger than the maximum %lub.", length, MAX_CHUNK_SIZE);
		return 0;
	}
	
	// Lookup a sufficiently large chunk in the free chunk table.
	int_fast32_t id = alloc_chunk_id(length);
	FreeMemoryChunk * m = LOCATE(FreeMemoryChunk, CHUNK_TABLE_LOCATION(id));
	while (m->next < HEADER_SIZE && ++id < CHUNK_TABLE_SIZE) {
		m = LOCATE(FreeMemoryChunk, CHUNK_TABLE_LOCATION(id));
	}
	
	dagdb_pointer r;
	length = dagdb_round_up(length);
	if (id < CHUNK_TABLE_SIZE) {
		// There is a sufficiently large chunk available.
		r = m->next;
		assert(r>=HEADER_SIZE);
		dagdb_chunk_remove(r);
		m = LOCATE(FreeMemoryChunk, r);
		assert(id==0 || free_chunk_id(m->size)==id);
		assert(id==0 || m->size==*LOCATE(dagdb_size,r+m->size-S));
		if (id > 0 && m->size-length>=MIN_CHUNK_SIZE) {
			dagdb_chunk_insert(r+length, m->size-length);
		}
	} else {
		// Allocate the memory in a newly created slab.
		r = dagdb_database_size;
		assert((dagdb_database_size % SLAB_SIZE) == 0);
		dagdb_size new_size = dagdb_database_size + SLAB_SIZE;
		if (new_size > MAX_SIZE) {
			dagdb_errno = DAGDB_ERROR_DB_TOO_LARGE;
			dagdb_report("Cannot enlarge database of %lub with %ub beyond hard coded limit of %u bytes", dagdb_database_size, SLAB_SIZE, MAX_SIZE);
			return 0;
		}
		if (ftruncate(dagdb_database_fd, new_size)) {
			dagdb_errno = DAGDB_ERROR_DB_TOO_LARGE;
			dagdb_report_p("Failed to grow database file to %lub", new_size);
			return 0;
		}
		dagdb_database_size = new_size;
		
		// Insert the unused part of the slab in the free chunk table.
		dagdb_chunk_insert(r+length, SLAB_USEABLE_SPACE_SIZE-length);
	}
	assert((r % S) == 0);
	// Mark the bitmap.
	dagdb_bitmap_mark(r,length,1);
#ifdef DAGDB_HARDEN_MALLOC
	for (uint64_t i=0; i<length; i+=8) {
		*LOCATE(uint64_t, r+i) = random();
	}
#endif // DAGDB_HARDEN_MALLOC
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
dagdb_pointer dagdb_realloc(dagdb_pointer location, dagdb_size oldlength, dagdb_size newlength) {
	return 0;
}

#define CHECK_BIT(a) (((a)<0) || ((a)>=BITMAP_SIZE) || dagdb_bitarray_read(slab->bitmap,(a)))

/**
 * Frees the provided memory.
 * Currently only zero's out the memory range.
 * This function also strips off the type information before freeing.
 * The chunk is put back in a pool, such that it can be reused by malloc. 
 * If free chunks are next to the chunk being released, then these chunks are merged.
 * If the last chunk used in the last slab is removed, then that slab is,
 * and all empty slabs that come directly before that are, truncated.
 */
void dagdb_free(dagdb_pointer location, dagdb_size length) {
	// Strip type information
	location &= ~DAGDB_TYPE_MASK;
	// Do sanity checks
	assert(location>=HEADER_SIZE);
	assert(location+length<=dagdb_database_size);
	assert(location % SLAB_SIZE + length <= SLAB_USEABLE_SPACE_SIZE);
	// Clear memory
	length = dagdb_round_up(length);
	memset(dagdb_file + location, 0, dagdb_round_up(length));
	
	// Free range in bitmap.
	dagdb_bitmap_mark(location, length, 0);
	
	// TODO (med): move left and right chunk elimination to a separate function, such that it can be reused in realloc.
	int_fast32_t bit, size;
	// Check for free chunk left.
	MemorySlab * slab = LOCATE(MemorySlab, location & ~(SLAB_SIZE-1));
	bit = (location % SLAB_SIZE)/S;
	size = 0;
	if (!CHECK_BIT(bit-1)) {
		if (!CHECK_BIT(bit-2)) {
			if (!CHECK_BIT(bit-3)) {
				size = *LOCATE(dagdb_size, location - S);
				assert(size>=3*S);
				assert(size<=location%SLAB_SIZE);
				assert(size==*LOCATE(dagdb_size, location - size + 2*S));
			} else {
				size = 2*S;
			}
			dagdb_chunk_remove(location - size);
		} else {
			size = S;
		}
	}
	location -= size;
	length += size;

	// Check for free chunk right.
	bit = (location % SLAB_SIZE + length)/S;
	size = 0;
	if (!CHECK_BIT(bit)) {
		if (!CHECK_BIT(bit+1)) {
			if (!CHECK_BIT(bit+2)) {
				size = LOCATE(FreeMemoryChunk, location + length)->size;
				assert(size>=3*S);
				assert(location%SLAB_SIZE + length + size<=SLAB_USEABLE_SPACE_SIZE);
				assert(size==*LOCATE(dagdb_size, location + length + size - S));
			} else {
				size = 2*S;
			}
			dagdb_chunk_remove(location + length);
		} else {
			size = S;
		}
	}
	length += size;

	// Add chunk to free chunk table.
	dagdb_chunk_insert(location, length);
	
	// Reduce file size if possible.
	dagdb_size new_size = dagdb_database_size;
	while (SLAB_USEABLE_SPACE_SIZE == *LOCATE(dagdb_size, new_size - SLAB_SIZE + SLAB_USEABLE_SPACE_SIZE - S)) {
		new_size -= SLAB_SIZE;
		// Remove chunk from table.
		dagdb_chunk_remove(new_size);
	}
	assert(new_size >= SLAB_SIZE);
	assert(new_size <= dagdb_database_size);
	assert(new_size % SLAB_SIZE == 0);
	if (new_size < dagdb_database_size) {
		if (ftruncate(dagdb_database_fd, new_size)) {
			dagdb_errno = DAGDB_ERROR_OTHER;
			dagdb_report_p("Failed to shrink database file to %lub", new_size);
		}
		dagdb_database_size = new_size;
	}
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
	for (int_fast32_t i=0; i<CHUNK_TABLE_SIZE; i++) {
		dagdb_pointer pos = CHUNK_TABLE_LOCATION(i);
		assert(pos < HEADER_SIZE);
		FreeMemoryChunk * m = LOCATE(FreeMemoryChunk, pos);
		m->prev = m->next = CHUNK_TABLE_LOCATION(i);
	}
	
	// Insert the unused part of the slab in the free chunk table.
	dagdb_chunk_insert(HEADER_SIZE, SLAB_USEABLE_SPACE_SIZE-HEADER_SIZE);
	
	// Mark header as used in bitmap
	dagdb_bitmap_mark(0, HEADER_SIZE, 1);
}

/**
 * Opens the given file. Creates it if it does not yet exist.
 * @returns 0 if successful.
 */
int dagdb_load(const char *database) {
	// Open the database file
	int_fast32_t fd = open(database, O_RDWR | O_CREAT, 0644);
	if (fd == -1) {
		dagdb_report_p("Cannot open '%s'", database);
		goto error;
	}

	// Map database into memory
	dagdb_file = mmap(NULL, MAX_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (dagdb_file == MAP_FAILED) {
		dagdb_report_p("Cannot map file to memory");
		goto error;
	}

	// Obtain length of database file
	dagdb_database_size = lseek(fd, 0, SEEK_END);
	if (dagdb_database_size>MAX_SIZE) {
		dagdb_report("File exceeds hardcoded size limit %lu > %u", dagdb_database_size, MAX_SIZE);
		goto error;
	}
	
	// database size must be a multiple of SLAB_SIZE
	if((dagdb_database_size%SLAB_SIZE)!=0) {
		dagdb_report("File has unexpected size %lu", dagdb_database_size);
		goto error;
	}
	
	Header* h = LOCATE(Header,0);
	if (dagdb_database_size == 0) {
		// Database is freshly created, initialize header.
		dagdb_database_fd = fd;
		if (ftruncate(fd, SLAB_SIZE)) {
			dagdb_report_p("Could not allocate %db diskspace for database", SLAB_SIZE);
			goto error;
		}
		dagdb_database_size = SLAB_SIZE;
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
		dagdb_database_fd = fd;
	}

	// Database opened successfully.
	return 0;

error:
	// Something went wrong, so cleanup.
	// At this point the error message must have been set already.
	dagdb_errno = DAGDB_ERROR_INVALID_DB;
	if (dagdb_file!=MAP_FAILED) {
		munmap(dagdb_file, MAX_SIZE);
		dagdb_file = MAP_FAILED;
	}
	if (fd != -1) close(fd);
	return -1;
}

/**
 * Closes the current database file.
 * Does nothing if no database file is currently open.
 */
void dagdb_unload() {
	if (dagdb_file!=MAP_FAILED) {
		munmap(dagdb_file, MAX_SIZE);
		dagdb_file = MAP_FAILED;
		//printf("Unmapped DB\n");
	}
	if (dagdb_database_fd) {
		close(dagdb_database_fd);
		dagdb_database_fd = 0;
		//printf("Closed DB\n");
	}
}
