#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <string.h>
#include <gcrypt.h>

#include "base.h"

static int storage[DAGDB_MAX_TYPE];

int dagdb_init(const char * root) {
	int i;
	int dir;
	if (root) {
		if (mkdir(root, 0755)) {
			if (errno != EEXIST) {
				return -1;
			}
		}
		dir = open(root, O_DIRECTORY | O_RDONLY);
		if (dir==-1) {
			return -1;
		}
	} else {
		dir = AT_FDCWD;
	}
	for (i = 0; i < DAGDB_MAX_TYPE; i++) {
		storage[i] = openat(dir, dagdb_filenames[i], O_CREAT | O_RDWR, 0644);
		if (storage[i] == -1) {
			return -1;
		}
	}
	if (dir!=AT_FDCWD) {
		close(dir);
	}
	return 0;
}

int dagdb_nibble(dagdb_hash h, int index) {
	return (h[index/8] >> (7-index%8)*4) & 0xf;
}

static int find(dagdb_hash h, dagdb_pointer p, dagdb_pointer * create = NULL) {
	int i;
	dagdb_trie t;
	for (i=0; i<20 & p.type==DAGDB_TRIE; i++) {
		lseek(storage[DAGDB_TRIE], SEEK_CUR, p.address);
		read(storage[DAGDB_TRIE],&t,sizeof(t));
		
	}
}

int dagdb_insert_data(void * data, uint64_t length) {
	dagdb_hash h;
	gcry_md_hash_buffer(GCRY_MD_SHA1, data, h, length);
	
	
	// insert the data in the data file.
	int64_t pos = lseek(storage[DAGDB_DATA], SEEK_END, 0);
	if (pos==-1) return -1;
	if (write(storage[DAGDB_DATA],&length,sizeof(length))) return -1;
	if (write(storage[DAGDB_DATA],&data,length)) return -1;
	dagdb_pointer p={DAGDB_DATA, pos};
	
	return 0;
}

