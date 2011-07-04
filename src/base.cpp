#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <string.h>
#include <gcrypt.h>
#include <assert.h>
#include <stdio.h>

#include "base.h"

/**
 * File handles for the storage files used by the database.
 */
static int storage[DAGDB_MAX_TYPE];

/**
 * Copies a hash.
 */
#define copy_hash(dest, src) memcpy((dest), (src), sizeof(dagdb_hash))

/**
 * Compares a hash.
 */
#define compare_hash(s1, s2) memcmp((s1), (s2), sizeof(dagdb_hash))

/**
 * Reads the item at the given location.
 * Breaks out of the calling function (returning -1) in case of an error.
 */
#define dagdb_read(item, pointer) if (pread(storage[(pointer).type],&(item),sizeof(item),(pointer).address)!=sizeof(item)) return -1

/**
 * Writes the given item at the given location.
 * Breaks out of the calling function (returning -1) in case of an error.
 */
#define dagdb_write(item, pointer) if (pwrite(storage[(pointer).type],&(item),sizeof(item),(pointer).address)!=sizeof(item)) return -1

/**
 * Calculates the offset of a pointer at the given position in the trie.
 */
#define TRIE_POINTER_OFFSET(index) ((int)&(((dagdb_trie*)NULL)->pointers[(index)]))

/// A constant denoting a trie node that does not point to anything
static const dagdb_trie empty_trie = {};

/// Used for converting to hexadecimal format
static const char * hex = "0123456789abcdef";

#ifdef NDEBUG
#	define log(...)
#else
	/**
	 * Used for logging. Has the same type as printf.
	 */
	static int (*logfn) (const char *,...) = NULL;
#	define log(...) if (logfn) logfn(__VA_ARGS__)
#endif

/**
 * Changes the method used for logging. 
 * @param f, the new logging method with the same signature as printf. Can be NULL to disable logging.
 * @warning If DAGDB has been compiled with NDEBUG defined, it won't write any logging.
 */
void dagdb_set_log_function(int (*f) (const char *,...)) {
#ifdef NDEBUG
	if (f) f("Warning: logging is not compiled into current DAGDB library.\n")
#else
	logfn = f;
#endif
}

/**
 * Adds a new trie node to the database.
 */
static int create_trie(dagdb_pointer * ptr, dagdb_trie t) {
	int64_t pos = lseek(storage[DAGDB_TRIE], 0, SEEK_END);
	if (write(storage[DAGDB_TRIE], &t, sizeof(t)) != sizeof(t))
		return -1;
	ptr->type = DAGDB_TRIE;
	ptr->address = pos;
	log("Created trie at %Lx\n",pos);
	return 0;
}

/**
 * Opens the files required by the database. Creating them if they do not yet exist.
 * 
 * \param root The root directory of the database. Can be relative to or
 * NULL for current working directory, The directory is created if it does 
 * not yet exist.
 */
int dagdb_init(const char * root) {
	int i;
	int dir;
	int size[DAGDB_MAX_TYPE];
	log("Initializing database\n");
	if (root) {
		if (mkdir(root, 0755)) {
			if (errno != EEXIST) {
				return -1;
			}
		} else {
			log("Created directory '%s'\n", root);
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
		int64_t pos = lseek(storage[i], 0, SEEK_END);
		if (pos==-1) return -1;
		size[i] = pos;
		log("Size of '%s' is %Ld\n", dagdb_filenames[i], pos);
	}
	if (dir!=AT_FDCWD) {
		close(dir);
	}
	if (size[DAGDB_TRIE]==0) {
		dagdb_pointer root;
		if (create_trie(&root, empty_trie)==-1)
			return -1;
		assert(root.address == 0);
	}
	return 0;
}

/**
 * Removes all contents from the db.
 */
int dagdb_truncate() {
	int i;
	for (i = 0; i < DAGDB_MAX_TYPE; i++) {
		if (ftruncate(storage[i],0)==-1) return -1;
	}
	
	log("Truncated DB\n");
	dagdb_pointer root;
	if (create_trie(&root, empty_trie)==-1)
		return -1;
	assert(root.address == 0);
	return 0;
}

/**
 * Gets the value of the 'index'-th character of the hexadecimal representation of the hash.
 */
int dagdb_nibble(const dagdb_hash h, int index) {
	assert(index>=0);
	assert(index<40);
	return (h[index/2] >> (1-index%2)*4) & 0xf;
	//return (h[index/8] >> (7-index%8)*4) & 0xf;
}

/**
 * Attempts to read the hash of the element pointed to by 'p'. 
 * If this fails or this element does not have a hash, -1 is returned.
 * The hash is stored in 'h'.
 * Pointers that have a hash are of type DAGDB_ELEMENT or DAGDB_KVPAIR.
 */
static int read_hash(dagdb_hash h, dagdb_pointer p) {
	if (p.type == DAGDB_KVPAIR) {
		dagdb_kvpair k; 
		dagdb_read(k,p);
		p = k.key;
		assert(p.type == DAGDB_ELEMENT);
	}
	if (p.type == DAGDB_ELEMENT) {
		dagdb_element e;
		dagdb_read(e,p);
		copy_hash(h, e.hash);
		return 0;
	}
	return -1;
}

/**
 * Searches the given hash in the trie located at the given root. The resulting pointer is 
 * stored in result. If the hash does not exist, result is set to NULL. 
 * @returns -1 in case of an error, 0 otherwise.
 */
int dagdb_find(dagdb_pointer * result, dagdb_hash h, dagdb_pointer root) {
	int i=0;
	dagdb_trie t;
	assert(result);
	assert(h);
	while(1) {
		if (root.type!=DAGDB_TRIE) {
			// element found, checking if hash matches.
			dagdb_hash hh;
			if(read_hash(hh, root)==-1) return -1;
			if(compare_hash(h,hh)==0) {
				*result = root;
			} else {
				// hash mismatch, return a NULL pointer.
				result->type=0;
				result->address=0;
			}
			return 0;
		}
		
		// Navigate one node further in the trie.
		if (i>=40) break;
		dagdb_read(t,root);
		root = t.pointers[dagdb_nibble(h,i)];
		
		if (root.type==0 && root.address==0) {
			// NULL pointer hit.
			*result = root;
			return 0;
		}
		i++;
	}
	// The loop above should never terminate normally.
	assert(0);
	return -1;
}

static int is_root(dagdb_pointer t) {
	return t.type==0 && t.address==0;
}

/**
 * Creates a new entry in the map which is pointed to by the pointer at 'root_location' and returns the 
 * location in 'result_location' where the pointer to the value of the entry must be stored.
 * @returns 0 if entry was created successfully, 1 if entry already exists and -1 in case of an error.
 */
static int insert(dagdb_pointer * result_location, dagdb_hash h, dagdb_pointer root_location) {
	int i=0;
	assert(result_location);
	assert(h);
	while(1) {
		dagdb_pointer p;
		if (is_root(root_location) && i==0) {
			// We are looking in the root trie. As there is no pointer to this trie, we use this hack.
			p = dagdb_root;
		} else {
			dagdb_read(p, root_location);
			if (is_root(p)) {
				// a NULL pointer is stored here. That means that the new entry can be inserted here.
				*result_location = root_location;
				return 0;
			}
		}
		
		if (p.type!=DAGDB_TRIE) {
			// There is already an element stored at this location. This 
			// element might either be the one we are trying to insert 
			// (in that case return 1) or a different element, which means
			// we need to create additional trie nodes and move the existing pointer.
			dagdb_hash hh;
			if (read_hash(hh, p)==-1) return -1;
			
			if(compare_hash(h,hh)==0) {
				// entry already exists in db.
				*result_location = root_location;
				return 1;
			} else {
				// hash mismatch, create new trie node.
				dagdb_pointer nt;
				dagdb_trie trie = empty_trie;
				
				// store the pointer to the old entry.
				trie.pointers[dagdb_nibble(hh, i)] = p;
				
				// write the trie to disk
				if(create_trie(&nt, trie)==-1) return -1;
				
				// Update the pointer in the current trie.
				dagdb_write(nt,root_location); // corrupts DB if interrupted.
				
				// Update our navigational pointer to contain the new value at 'root_pointer'.
				p = nt;
				
				// return to the search algorithm.
			}
		}
		
		// We are still looking at a trie node. Calculate the address of the next pointer.
		if (i>=40) goto error;
		p.address += TRIE_POINTER_OFFSET(dagdb_nibble(h, i));
		root_location = p;
		i++;
	}
	
	error:
	log(" i=%d ", i);
	// The nibbles in the hash are exhausted. There must be something wrong.
	assert(0);
	return -1;
}

/**
 * Calculates the hash of a bytestring.
 */
void dagdb_get_hash(dagdb_hash h, const void * data, int length) {
	gcry_md_hash_buffer(GCRY_MD_SHA1, h, data, length);
}

/**
 * Inserts the given data into the root trie of the database.
 * @returns 0 if entry was created successfully, 1 if entry already exists and -1 in case of an error.
 */
int dagdb_insert_data(const void * data, uint64_t length) {
	int64_t pos;
	dagdb_hash h;
	dagdb_get_hash(h, data, length);
	
	// Create an entry in our root trie.
	dagdb_pointer location;
	int r = insert(&location, h, dagdb_root);
	if (r) return r; // error or entry already exists.
	
	// insert the data in the data file.
	pos = lseek(storage[DAGDB_DATA], 0, SEEK_END);
	if (pos==-1) return -1;
	if (write(storage[DAGDB_DATA],&length,sizeof(length)) != sizeof(length)) return -1;
	if ((uint64_t)write(storage[DAGDB_DATA],data,length) != length) return -1;
	dagdb_pointer p={DAGDB_DATA, pos};
	
	// Create an element for our data.
	dagdb_element e;
	copy_hash(e.hash, h);
	e.forward = p;
	e.reverse = dagdb_root;
	pos = lseek(storage[DAGDB_ELEMENT], 0, SEEK_END);
	if (pos==-1) return -1;
	dagdb_pointer q={DAGDB_ELEMENT, pos};
	dagdb_write(e, q);
	
	// Write the pointer to our new entry.
	dagdb_write(q, location);
	
	log("Inserted %Ld bytes of data.\n", length);
	return 0;
}

static int value(char c) {
	if (c>='0' && c<='9') return c-'0';
	if (c>='a' && c<='f') return c-'a'+10;
	return 0;
}

/**
 * Converts a string of hexadecimal numbers into a hash.
 */
void dagdb_parse_hash(dagdb_hash h, const char * t) {
	int i;
	for (i=0; i<20; i++) {
		h[i] = value(t[i*2])*16 + value(t[i*2+1]);
	}
}

/**
 * Converts a hash into a string of hexadecimal numbers.
 */
void dagdb_write_hash(char * t, const dagdb_hash h) {
	int i;
	for (i=0; i<40; i++) {
		t[i]=hex[dagdb_nibble(h,i)];
	}
	t[40]=0;
}

/**
 * @returns the length of the pointed data object or -1 in case of an error.
 */
int64_t dagdb_length(dagdb_pointer p) {
	if (p.type==DAGDB_DATA) {
		dagdb_data d;
		dagdb_read(d, p);
		return d.length;
	}
	return -1;
}

/** 
 * Reads an element of type element.
 */
int dagdb_read_element(dagdb_element * element, dagdb_pointer location) {
	dagdb_read(*element, location); 
	return 0;
}

