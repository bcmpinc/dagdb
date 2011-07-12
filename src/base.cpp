#include <cerrno>
#include <cstring>
#include <cassert>
#include <cstdio>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <gcrypt.h>

#include "base.h"

namespace Dagdb {//

// TODO: make part of blob
const char * filenames[] = {"tries","elements","data","kvpairs"};
const int filename_length = 8;
//const Pointer root = {Type::trie, 0};

/**
 * File handles for the storage files used by the database.
 */
int storage[(int)Type::__max_type];

/// A constant denoting a trie node that does not point to anything
static const Trie empty_trie = {};

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
void set_log_function(int (*f) (const char *,...)) {
#ifdef NDEBUG
	if (f) f("Warning: logging is not compiled into current DAGDB library.\n")
#else
	logfn = f;
#endif
}

#define DAGDB_MAX_TYPE ((int)Type::__max_type)

/**
 * Reads the item at the given location.
 * Breaks out of the calling function (returning -1) in case of an error.
 */
template<class T>
int Blob<T>::read(Pointer p) { 
	return pread(storage[(int)p.type],this,sizeof(T),p.address)!=sizeof(T);
}
/**
 * Writes the given item at the given location.
 * Breaks out of the calling function (returning -1) in case of an error.
 */
template<class T>
int Blob<T>::write(Pointer p) const {
	return pwrite(storage[(int)p.type],this,sizeof(T),p.address)!=sizeof(T);
}

// TODO: specialize Data and make data part a pointer.

/**
 * Appends a new T to the database.
 */
// TODO: return pointer or throw error.
// TODO: get rid of Type argument
template<class T>
int Blob<T>::append(Pointer *ptr, Type t) const {
	int64_t pos = lseek(storage[(int)t], 0, SEEK_END);
	ptr->type = t;
	ptr->address = pos;
	if (write(*ptr)==-1) return -1;
	log("Appended %d at %Lx\n",(int)t,pos);
	return 0;
}

/**
 * Calculates the offset of a pointer at the given position in the trie.
 */
#define TRIE_POINTER_OFFSET(index) ((int)&(((Trie*)NULL)->pointers[(index)]))

/**
 * Opens the files required by the database. Creating them if they do not yet exist.
 * 
 * \param root The root directory of the database. Can be relative to or
 * NULL for current working directory, The directory is created if it does 
 * not yet exist.
 */
// TODO: throw error.
int init(const char * root_dir) {
	int dir;
	int size[(int)Type::__max_type];
	log("Initializing database\n");
	if (root_dir) {
		if (mkdir(root_dir, 0755)) {
			if (errno != EEXIST) {
				return -1;
			}
		} else {
			log("Created directory '%s'\n", root_dir);
		}
		dir = open(root_dir, O_DIRECTORY | O_RDONLY);
		if (dir==-1) {
			return -1;
		}
	} else {
		dir = AT_FDCWD;
	}
	for (int i = 0; i < DAGDB_MAX_TYPE; i++) {
		storage[i] = openat(dir, filenames[i], O_CREAT | O_RDWR, 0644);
		if (storage[i] == -1) {
			return -1;
		}
		int64_t pos = lseek(storage[i], 0, SEEK_END);
		if (pos==-1) return -1;
		size[i] = pos;
		log("Size of '%s' is %Ld\n", filenames[i], pos);
	}
	if (dir!=AT_FDCWD) {
		close(dir);
	}
	if (size[(int)Type::trie]==0) {
		Pointer r;
		if (empty_trie.append(&r, Type::trie)==-1)
			return -1;
		assert(r == root);
	}
	return 0;
}

/**
 * Removes all contents from the db.
 */
int truncate() {
	int i;
	for (i = 0; i < DAGDB_MAX_TYPE; i++) {
		if (ftruncate(storage[i],0)==-1) return -1;
	}
	
	log("Truncated DB\n");
	Pointer r;
	if (empty_trie.append(&r, Type::trie)==-1)
		return -1;
	assert(r == root);
	return 0;
}

/**
 * Gets the value of the 'index'-th character of the hexadecimal representation of the hash.
 */
int Hash::nibble(int index) const {
	assert(index>=0);
	assert(index<40);
	return (byte[index/2] >> (1-index%2)*4) & 0xf;
}

/**
 * Attempts to read the hash of the element pointed to by 'p'. 
 * If this fails or this element does not have a hash, -1 is returned.
 * The hash is stored in 'h'.
 * Pointers that have a hash are of type DAGDB_ELEMENT or DAGDB_KVPAIR.
 */
// TODO: move into blob classes.
static int read_hash(Hash h, Pointer p) {
/*	if (p.type == DAGDB_KVPAIR) {
		kvpair k; 
		read(k,p);
		p = k.key;
		assert(p.type == DAGDB_ELEMENT);
	}
	if (p.type == DAGDB_ELEMENT) {
		element e;
		read(e,p);
		copy_hash(h, e.hash);
		return 0;
	}*/
	return -1;
}

/**
 * Searches the given hash in the trie located at the given root. The resulting pointer is 
 * stored in result. If the hash does not exist, result is set to NULL. 
 * @returns -1 in case of an error, 0 otherwise.
 */
// TODO: return pointer or throw error.
int Pointer::find(Pointer * result, Hash h) const {
	int i=0;
	Trie t;
	assert(result);
	Pointer cur = *this;
	while(1) {
		if (cur.type!=Type::trie) {
			// element found, checking if hash matches.
			Hash hh;
			if(read_hash(hh, cur)==-1) return -1;
			if(h==hh) {
				*result = cur;
			} else {
				// hash mismatch, return a NULL pointer (ie. root).
				*result = root;
			}
			return 0;
		}
		
		// Navigate one node further in the trie.
		if (i>=40) break;
		t.read(cur);
		cur = t.pointers[h.nibble(i)];
		
		if (cur == root) {
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

/**
 * Creates a new entry in the map which is pointed to by the pointer at 'root_location' and returns the 
 * location in 'result_location' where the pointer to the value of the entry must be stored.
 * @returns 0 if entry was created successfully, 1 if entry already exists and -1 in case of an error.
 */
// TODO: return pointer or throw error.
int Pointer::insert(Pointer * result_location, Hash h) const {
	int i=0;
	assert(result_location);
	Pointer cur = *this;
	while(1) {
		Pointer p;
		if (cur == root && i == 0) {
			// We are looking in the root trie. As there is no pointer to this trie, we use this hack.
			p = root;
		} else {
			p.read(cur);
			if (p == root) {
				// a NULL pointer is stored here. That means that the new entry can be inserted here.
				*result_location = cur;
				return 0;
			}
		}
		
		if (p.type != Type::trie) {
			// There is already an element stored at this location. This 
			// element might either be the one we are trying to insert 
			// (in that case return 1) or a different element, which means
			// we need to create additional trie nodes and move the existing pointer.
			Hash hh;
			if (p.type == Type::element) {
				if (hh.read(p)) return -1;
			} else {
				return -1; // KVpairs not yet supported.
			}
			
			if(h == hh) {
				// entry already exists in db.
				*result_location = cur;
				return 1;
			} else {
				// hash mismatch, create new trie node.
				Pointer nt;
				Trie trie = empty_trie;
				
				// store the pointer to the old entry.
				trie.pointers[hh.nibble(i)] = p;
				
				// write the trie to disk
				if (empty_trie.append(&nt, Type::trie)==-1)
					return -1;
				
				// Update the pointer in the current trie.
				nt.write(cur); // corrupts DB if interrupted.
				
				// Update our navigational pointer to contain the new value at 'root_pointer'.
				p = nt;
				
				// return to the search algorithm.
			}
		}
		
		// We are still looking at a trie node. Calculate the address of the next pointer.
		if (i>=40) goto error;
		p.address += TRIE_POINTER_OFFSET(h.nibble(i));
		cur = p;
		i++;
	}
	
	error:
	log(" i=%d ", i);
	// The nibbles in the hash are exhausted. There must be something wrong.
	assert(0);
	return -1;
}

Pointer::Pointer(Type type, uint64_t address) : type(type), address(address) {
}


/**
 * Calculates the hash of a bytestring.
 */
// TODO: move into Hash constructor or add as method for blobs
void get_hash(Hash h, const void * data, int length) {
	gcry_md_hash_buffer(GCRY_MD_SHA1, h.byte, data, length);
}

/**
 * Inserts the given data into the root trie of the database.
 * @returns 0 if entry was created successfully, 1 if entry already exists and -1 in case of an error.
 */
// TODO: make more generic?
int insert_data(const void * data, uint64_t length) {
	int64_t pos;
	Hash h;
	get_hash(h, data, length);
	
	// Create an entry in our root trie.
	Pointer location;
	int r = root.insert(&location, h);
	if (r) return r; // error or entry already exists.
	
	// insert the data in the data file.
	pos = lseek(storage[(int)Type::data], 0, SEEK_END);
	if (pos==-1) return -1;
	if (write(storage[(int)Type::data],&length,sizeof(length)) != sizeof(length)) return -1;
	if ((uint64_t)write(storage[(int)Type::data],data,length) != length) return -1;
	
	
	// Create an element for our data.
	Element e;
	e.hash = h;
	e.forward = Pointer(Type::data, pos);
	e.reverse = root;
	pos = lseek(storage[(int)Type::element], 0, SEEK_END);
	if (pos==-1) return -1;
	Pointer q(Type::element, pos);
	e.write(q);
	
	// Write the pointer to our new entry.
	q.write(location);
	
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
void Hash::parse(const char * t) {
	int i;
	for (i=0; i<20; i++) {
		byte[i] = value(t[i*2])*16 + value(t[i*2+1]);
	}
}

/**
 * Converts a hash into a string of hexadecimal numbers.
 */
void Hash::write(char * t) const {
	int i;
	for (i=0; i<40; i++) {
		t[i]=hex[nibble(i)];
	}
	t[40]=0;
}

/**
 * @returns the length of the pointed data object or -1 in case of an error.
 */
// TODO: remove in favor of read/write specialization? What about large blobs and random io?
int64_t Pointer::data_length() {
	if (type==Type::data) {
		Data d;
		d.read(*this);
		return d.length;
	}
	return -1;
}

};
