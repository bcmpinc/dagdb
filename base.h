#ifndef DAGDB_BASE_H
#define DAGDB_BASE_H
#include <stdint.h>

#define DAGDB_TRIE    0
#define DAGDB_ELEMENT 1
#define DAGDB_DATA    2
#define DAGDB_KVPAIR  3
// ---
#define DAGDB_MAX_TYPE   4

const char * dagdb_filenames[] = {"tries","elements","data","kvpairs"};
#define DAGDB_MAX_FILENAME_LENGTH 8

typedef uint8_t dagdb_hash[20];

typedef struct {
	uint64_t type : 8;
	uint64_t address : 56;
} dagdb_pointer;

typedef struct {
	dagdb_hash hash;
	dagdb_pointer forward;
	dagdb_pointer reverse;
} dagdb_element;

typedef struct {
	int64_t length;
	uint8_t data[0];
} dagdb_data;

typedef struct {
	dagdb_pointer pointers[16];
} dagdb_trie;

typedef struct {
	dagdb_pointer key; // must point to an dagdb_element.
	dagdb_pointer value;
} dagdb_kvpair;

int dagdb_nibble(dagdb_hash h, int index);
void dagdb_parse_hash(dagdb_hash h, char * t);
void dagdb_write_hash(char * t, dagdb_hash h);
void dagdb_get_hash(dagdb_hash h, void * data, int length);

void dagdb_set_log_function(int (*f) (const char *,...));
int dagdb_init(const char * root);
int dagdb_truncate();

int dagdb_insert_data(void * data, uint64_t length);
int dagdb_find(dagdb_pointer * result, dagdb_hash h, dagdb_pointer root);

int64_t dagdb_length(dagdb_pointer * p);

const dagdb_pointer dagdb_root = {0,0};

#endif
