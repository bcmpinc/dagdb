
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "base2.h"

#define MAX_SIZE (1<<30)
#define LOCATE(type,location) (*(type*)(file+location))
#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1]

#define S (sizeof(dagdb_size))
STATIC_ASSERT(S == sizeof(dagdb_size), same_pointer_size_size);
STATIC_ASSERT(((S - 1)&S) == 0, size_power_of_two);

static int database_fd;
static void *file;
static dagdb_size size;

dagdb_size dagdb_round_up(dagdb_size v) {
	return  -(~(sizeof(dagdb_pointer) - 1) & -v);
}

/**
 * Allocates the requested amount of bytes.
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

static void dagdb_free(dagdb_pointer location, dagdb_size length) {
	memset(file + location, 0, dagdb_round_up(length));
	// TODO:
}

/**
 * Opens the given file.
 * @returns -1 on failure.
 */
int dagdb_load(const char *database) {
	int fd = open(database, O_RDWR | O_CREAT, 0644);
	if (fd == -1) goto error;

	file = mmap(NULL, MAX_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (file == MAP_FAILED) goto error;
	printf("Database data @ %p\n", file);

	size = lseek(fd, 0, SEEK_END);
	printf("Database size: %d\n", size);

	database_fd = fd;
	printf("Opened DB\n");
	return 0;

error:
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
		munmap(file, 1 << 24);
		file = 0;
	}
	if (database_fd) {
		close(database_fd);
		database_fd = 0;
		printf("Closed DB\n");
	}
}

/**
 * Data item.
 * S bytes: length
 * length bytes rounded up to a multiple of S: data
 */
dagdb_pointer dagdb_data_create(dagdb_size length, const void *data) {
	dagdb_pointer r = dagdb_malloc(length + S);
	LOCATE(dagdb_size, r) = length;
	memcpy(file + r + S, data, length);
	return r;
}

void dagdb_data_delete(dagdb_pointer location) {
	dagdb_size length = LOCATE(int, location);
	dagdb_free(location, length + S);
}

dagdb_size dagdb_data_length(dagdb_pointer location) {
	return LOCATE(int, location);
}

const void *dagdb_data_read(dagdb_pointer location) {
	return file + location + S;
}

/**
 * Element
 * DAGDB_KEY_LENGTH bytes: key
 * S bytes: forward pointer (data or trie)
 * S bytes: pointer to backref. (trie)
 */
dagdb_pointer dagdb_element_create(dagdb_key hash, dagdb_pointer pointer) {
	dagdb_pointer r = dagdb_malloc(DAGDB_KEY_LENGTH + 2 * S);
	memcpy(file + r, hash, DAGDB_KEY_LENGTH);
	LOCATE(dagdb_pointer, r + DAGDB_KEY_LENGTH) = pointer;
	LOCATE(dagdb_pointer, r + DAGDB_KEY_LENGTH + S) = 0;
	return r;
}

void dagdb_element_delete(dagdb_pointer location) {
	if (LOCATE(dagdb_pointer, location + DAGDB_KEY_LENGTH + S) != 0) {
		// TODO: destroy backref
	}
	dagdb_free(location, DAGDB_KEY_LENGTH + 2 * S);
}

dagdb_pointer dagdb_element_backref(dagdb_pointer location) {
	return location + DAGDB_KEY_LENGTH + S;
}

dagdb_pointer dagdb_element_data(dagdb_pointer location) {
	return LOCATE(dagdb_pointer, location + DAGDB_KEY_LENGTH);
}

/**
 * KV Pair
 * S bytes: Key (element)
 * S bytes: Value (element or trie)
 */
dagdb_pointer dagdb_kvpair_create(dagdb_pointer key, dagdb_pointer value) {
	dagdb_pointer r = dagdb_malloc(2 * S);
	LOCATE(dagdb_pointer, r) = key;
	LOCATE(dagdb_pointer, r + S) = value;
	return r;
}

void dagdb_kvpair_delete(dagdb_pointer location) {
	// TODO: destroy trie if exists.
	dagdb_free(location, 2 * S);
}

dagdb_pointer dagdb_kvpair_key(dagdb_pointer location) {
	return LOCATE(dagdb_pointer, location);
}

dagdb_pointer dagdb_kvpair_value(dagdb_pointer location) {
	return LOCATE(dagdb_pointer, location + S);
}



