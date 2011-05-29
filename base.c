#include <gcrypt.h>

#include "dagdb.h"

int dagdb_nibble(dagdb_hash h, int index) {
	return (h[index/8] >> (7-index%8)*4) & 0xf;
}

int dagdb_type(dagdb_pointer p) {
	return (p>>56) & 0xf;
}