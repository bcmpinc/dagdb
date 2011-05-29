#ifndef DAGDB_H
#define DAGDB_H
#include <stdint.h>

/** \file
 * The dagdb can store maps of the form hash -> data and
 * maps of the form hash -> map.
 */

#define DAGDB_ELEMENT 1
#define DAGDB_DATA    2
#define DAGDB_TREAP   3
#define DAGDB_KVPAIR  4

typedef int64_t dagdb_pointer;
typedef int32_t dagdb_hash[5];

typedef struct {
	dagdb_hash hash;
	dagdb_pointer forward;
	dagdb_pointer reverse;
} dagdb_element;

typedef struct {
	int64_t length;
	int8_t data[0];
} dagdb_data;

typedef struct {
	dagdb_pointer pointers[16];
} dagdb_treap;

typedef struct {
	dagdb_pointer key;
	dagdb_pointer value;
} dagdb_kvpair;

int dagdb_nibble(dagdb_hash h, int index);
int dagdb_type(dagdb_pointer p);

int insert(dagdb_data *);

#endif
