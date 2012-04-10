
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "base2.h"

/////////////
// Macro's //
/////////////

/**
 * Maximum amount of space (in bytes) that the database is allowed to use.
 * TODO: Remove this limit.
 */
#define MAX_SIZE (1<<30)

/**
 * Masks the part of a pointer that contains type information.
 */
#define DAGDB_TYPE_MASK (S-1)

/**
 * Defines a pointer of type 'type', named 'var' pointing to the given location in the database file.
 * Also handles stripping off the pointers type information.
 */
#define LOCATE(type,var,location) type* var = (type*)(file+(location&~DAGDB_TYPE_MASK))

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
#define DAGDB_MAGIC (*(int*)"D-db")

//////////////////////
// Static variables //
//////////////////////

/**
 * The file descriptor of the currently opened database.
 */
static int database_fd;

/**
 * Pointer to the data of the file in memory.
 */
static void *file;

/**
 * The current size of the currently opened database.
 */
static dagdb_size size;

//////////////////
// Declarations //
//////////////////


///////////////////
// Pointer types //
///////////////////

STATIC_ASSERT(DAGDB_TYPE_DATA    < S,pointer_size_too_small_to_contain_type_data);
STATIC_ASSERT(DAGDB_TYPE_ELEMENT < S,pointer_size_too_small_to_contain_type_element);
STATIC_ASSERT(DAGDB_TYPE_TRIE    < S,pointer_size_too_small_to_contain_type_trie);
STATIC_ASSERT(DAGDB_TYPE_KVPAIR  < S,pointer_size_too_small_to_contain_type_kvpair);
inline dagdb_pointer_type dagdb_get_type(dagdb_pointer location) {
	return location&DAGDB_TYPE_MASK;
}

//////////////////////
// Space allocation //
//////////////////////

/**
 * Rounds up the given argument to a multiple of S.
 */
dagdb_size dagdb_round_up(dagdb_size v) {
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
 * Frees the requested memory.
 * Currently only zero's out the memory range.
 * This function also strips off the type information before freeing.
 */
static void dagdb_free(dagdb_pointer location, dagdb_size length) {
	// Strip type information
	location &= ~DAGDB_TYPE_MASK;
	// Do sanity checks
	assert(location>=HEADER_SIZE);
	assert(location+length<=size);
	// Clear memory
	memset(file + location, 0, dagdb_round_up(length));
	// TODO: add to memory pool & truncate file if possible.
}

//////////////////////
// Database loading //
//////////////////////

/**
 * Stores some basic information about the database.
 * The size of this header cannot exceed HEADER_SIZE.
 */
typedef struct {
	int magic;
	int format_version;
	dagdb_pointer root;
} Header;
STATIC_ASSERT(sizeof(Header) <= HEADER_SIZE, header_too_large);

/**
 * Opens the given file. Creates it if it does not yet exist.
 * @returns -1 on failure.
 */
int dagdb_load(const char *database) {
	// Open the database file
	int fd = open(database, O_RDWR | O_CREAT, 0644);
	if (fd == -1) goto error;
	printf("Database file opened %d\n", fd);

	// Map database into memory
	file = mmap(NULL, MAX_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (file == MAP_FAILED) goto error;
	printf("Database data @ %p\n", file);

	// Obtain length of database file
	size = lseek(fd, 0, SEEK_END);
	printf("Database size: %d\n", size);
	
	// Check or generate header information.
	LOCATE(Header,h,0);
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
		h->root=dagdb_trie_create();
		size = HEADER_SIZE;
	} else {
		// database size must be a multiple of S
		assert((size&DAGDB_TYPE_MASK)==0); 
		// check headers.
		assert(h->magic==DAGDB_MAGIC);
		assert(h->format_version==FORMAT_VERSION);
		printf("Database format %d accepted.\n", h->format_version);
		database_fd = fd;
	}

	printf("Opened DB\n");
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
		printf("Unmapped DB\n");
	}
	if (database_fd) {
		close(database_fd);
		database_fd = 0;
		printf("Closed DB\n");
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
	LOCATE(Data,d,r);
	d->length = length;
	memcpy(d->data, data, length);
	return r | DAGDB_TYPE_DATA;
}

void dagdb_data_delete(dagdb_pointer location) {
	assert(dagdb_get_type(location) == DAGDB_TYPE_DATA);
	LOCATE(Data,d,location);
	dagdb_free(location, sizeof(Data) + d->length);
}

dagdb_size dagdb_data_length(dagdb_pointer location) {
	assert(dagdb_get_type(location) == DAGDB_TYPE_DATA);
	LOCATE(Data,d,location);
	return d->length;
}

const void *dagdb_data_read(dagdb_pointer location) {
	assert(dagdb_get_type(location) == DAGDB_TYPE_DATA);
	LOCATE(Data,d,location);
	return d->data;
}


//////////////
// Elements //
//////////////

/**
 * Element
 * DAGDB_KEY_LENGTH bytes: key
 * S bytes: forward pointer (data or trie)
 * S bytes: pointer to backref. (trie)
 */
typedef struct {
	unsigned char key[DAGDB_KEY_LENGTH];
	dagdb_pointer data;
	dagdb_pointer backref;
} Element;

dagdb_pointer dagdb_element_create(dagdb_key hash, dagdb_pointer data, dagdb_pointer backref) {
	dagdb_pointer r = dagdb_malloc(sizeof(Element));
	LOCATE(Element, e, r);
	memcpy(e->key, hash, DAGDB_KEY_LENGTH);
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
	LOCATE(Element, e, location);
	return e->data;
}

dagdb_pointer dagdb_element_backref(dagdb_pointer location) {
	assert(dagdb_get_type(location) == DAGDB_TYPE_ELEMENT);
	LOCATE(Element, e, location);
	return e->backref;
}

//////////////
// KV Pairs //
//////////////

/**
 * KV Pair
 * S bytes: Key (element)
 * S bytes: Value (element or trie)
 */
typedef struct  {
	dagdb_pointer key;
	dagdb_pointer value;
} KVPair;

dagdb_pointer dagdb_kvpair_create(dagdb_pointer key, dagdb_pointer value) {
	assert(dagdb_get_type(key) == DAGDB_TYPE_ELEMENT);
	dagdb_pointer r = dagdb_malloc(sizeof(KVPair));
	LOCATE(KVPair, p, r);
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
	LOCATE(KVPair, p, location);
	return p->key;
}

dagdb_pointer dagdb_kvpair_value(dagdb_pointer location) {
	assert(dagdb_get_type(location) == DAGDB_TYPE_KVPAIR);
	LOCATE(KVPair, p, location);
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
	int i;
	LOCATE(Trie, t, location);
	for (i=0; i<16; i++) {
		if (dagdb_get_type(t->entry[i]) == DAGDB_TYPE_TRIE)
			dagdb_trie_delete(t->entry[i]);
	}
	dagdb_free(location, sizeof(Trie));
}

dagdb_pointer dagdb_trie_find(dagdb_pointer trie, dagdb_key key)
{
	assert(dagdb_get_type(trie) == DAGDB_TYPE_TRIE);
	

}

static int nibble(uint8_t * key, int index) {
	assert(index >= 0);
	assert(index < 2*DAGDB_KEY_LENGTH);
	if (index&1)
		return (key[index>>1]>>4)&0xf;
	else
		return key[index>>1]&0xf;
}

static key obtain_key(dagdb_pointer pointer) {
	// Obtain the key.
	if (dagdb_get_type(pointer)==DAGDB_TYPE_KVPAIR) {
		LOCATE(KVPair,kv,pointer);
		pointer = kv->key;
	}
	assert(dagdb_get_type(pointer)==DAGDB_TYPE_ELEMENT);
	LOCATE(Element,e,pointer);
	return e->key;
}

/**
 * Inserts the given pointer into the trie.
 * The pointer must be an Element or KVPair.
 * Returns 1 if the element is indeed added to the trie.
 * Returns 0 if the element was already in the trie.
 */
int dagdb_trie_insert(dagdb_pointer trie, dagdb_pointer pointer)
{
	assert(dagdb_get_type(trie) == DAGDB_TYPE_TRIE);
	
	key k = obtain_key(pointer);
	
	// Traverse the trie.
	int i;
	for(i=0;i<2*DAGDB_KEY_LENGTH;i++) {
		LOCATE(Trie, t, trie);
		int n = nibble(k, i);
		if (t->entry[n]==0) { 
			// Spot is empty, so we can insert it here.
			t->entry[n] = pointer;
			return 1;
		}
		if (dagdb_get_type(t->entry[n]) == DAGDB_TYPE_TRIE) {
			// Descend into the already existing sub-trie
			trie = t->entry[n];
		} else {
			// Check if the element here has the same hash.
			key l = obtain_key(t->entry[n]);
			int same = memcmp(k,l,DAGDB_KEY_LENGTH);
			if (same == 0) return 0;
			
			// Create new tries until we have a differing nibble
			int m=nibble(l,i);
			while (n == m) {
				dagdb_pointer newtrie = dagdb_trie_create();
				LOCATE(Trie, t2, newtrie);
				i++;
				
				// Push the existing element's pointer into the new trie.
				LOCATE(Trie, t, trie);
				t2->entry[m] = t->entry[m];
				t->entry[m] = newtrie;
				n = nibble(k,i);
				m = nibble(k,i);
				trie = t2;
			}
			LOCATE(Trie, t, trie);
			t->entry[n] = pointer;
		}
	}
	fprintf(stderr,"Unreachable state reached in '%s'\n", __func__);
	abort();
}

int dagdb_trie_remove(dagdb_pointer trie, dagdb_key hash)
{
	assert(dagdb_get_type(trie) == DAGDB_TYPE_TRIE);

}

dagdb_pointer dagdb_root()
{
	LOCATE(Header, h, 0);
	return h->root;
}
