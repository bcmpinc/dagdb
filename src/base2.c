
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
 * Defines a pointer of type 'type', named 'var' pointing to the given location in the database file.
 */
#define LOCATE(type,var,location) type* var = (type*)(file+location)

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
#define DAGDB_MAGIC ((int)*"D-db")

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
	dagdb_size alength = dagdb_round_up(length) + 8;
	size += alength;
	assert(size <= MAX_SIZE);
	if (ftruncate(database_fd, size)) {
		perror("dagdb_malloc");
		abort();
	}
	return r;
}

/**
 * Frees the requested memory.
 * Currently only zero's out the memory range.
 */
static void dagdb_free(dagdb_pointer location, dagdb_size length) {
	memset(file + location, 0, dagdb_round_up(length));
	// TODO:
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
 * Opens the given file.
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
		assert(size==0);
		if (ftruncate(fd, HEADER_SIZE)) {
			perror("dagdb_load init");
			abort();
		}
		h->magic=DAGDB_MAGIC;
		h->format_version=FORMAT_VERSION;
		size = HEADER_SIZE;
	} else {
		assert(h->magic==DAGDB_MAGIC);
		assert(h->format_version==FORMAT_VERSION);
		printf("Database format %d accepted.\n", h->format_version);
	}

	database_fd = fd;
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
	return r;
}

void dagdb_data_delete(dagdb_pointer location) {
	LOCATE(Data,d,location);
	dagdb_free(location, sizeof(Data) + d->length);
}

dagdb_size dagdb_data_length(dagdb_pointer location) {
	LOCATE(Data,d,location);
	return d->length;
}

const void *dagdb_data_read(dagdb_pointer location) {
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
	dagdb_pointer forward;
	dagdb_pointer backref;
} Element;

dagdb_pointer dagdb_element_create(dagdb_key hash, dagdb_pointer pointer) {
	dagdb_pointer r = dagdb_malloc(sizeof(Element));
	LOCATE(Element, e, r);
	memcpy(e->key, hash, DAGDB_KEY_LENGTH);
	e->forward = pointer;
	e->backref = 0;
	return r;
}

void dagdb_element_delete(dagdb_pointer location) {
	LOCATE(Element, e, location);
	dagdb_trie_destroy(e->backref);
	dagdb_free(location, sizeof(Element));
}

dagdb_pointer dagdb_element_backref(dagdb_pointer location) {
	return location + (int)&(((Element*)0)->backref);
}

dagdb_pointer dagdb_element_data(dagdb_pointer location) {
	LOCATE(Element, e, location);
	return e->forward;
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
	dagdb_pointer r = dagdb_malloc(sizeof(KVPair));
	LOCATE(KVPair, p, r);
	p->key = key;
	p->value = value;
	return r;
}

void dagdb_kvpair_delete(dagdb_pointer location) {
	dagdb_trie_destroy(location + S);
	dagdb_free(location, 2 * S);
}

dagdb_pointer dagdb_kvpair_key(dagdb_pointer location) {
	LOCATE(KVPair, p, location);
	return p->key;
}

dagdb_pointer dagdb_kvpair_value(dagdb_pointer location) {
	LOCATE(KVPair, p, location);
	return p->value;
}

///////////
// Tries //
///////////

/** 
 * The trie
 * 16 * S bytes: Pointers (trie or element)
 */
typedef dagdb_pointer Trie[16];

dagdb_pointer dagdb_trie_find(dagdb_pointer trie, dagdb_key hash)
{

}

void dagdb_trie_destroy(dagdb_pointer location)
{
	// FIXME: this is wrong!!!
	if (location==0) return;
	// TODO: check if this is pointing to a trie.
	int i;
	LOCATE(Trie, t, location);
	for (i=0; i<16; i++) {
		dagdb_trie_destroy(t[i]);
	}
	dagdb_free(location, S*16);
}

int dagdb_trie_insert(dagdb_pointer trie, dagdb_pointer pointer)
{

}

int dagdb_trie_remove(dagdb_pointer trie, dagdb_key hash)
{

}

dagdb_pointer dagdb_root()
{
	return &(((Header*)0)->root);
}
