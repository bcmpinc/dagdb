#include <cerrno>
#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <gcrypt.h>

#include "base.h"
#include "util.h"

namespace dagdb {//
namespace storage {//

/**
 * Keeps track of the various storage files used by the DB.
 */
static StorageInfo *storage[4];
static const int storage_size = sizeof(storage) / sizeof(storage[0]);
static const size_t max_filename_length = 8;

/**
 * Creates a new entry for storage.
 */
StorageInfo::StorageInfo(uint8_t type, const char *name) : type(type), name(name), handle(0) {
	assert(type < storage_size);
	assert(storage[type] == 0); // Verify that the type number is unique.
	assert(strlen(name) <= max_filename_length);
	storage[type] = this;
}

// List of storage files and parameters.
template<> const StorageInfo Storage<Trie>::info = StorageInfo(0, "tries");
template<> const StorageInfo Storage<Element>::info = StorageInfo(1, "elements");
const StorageInfo Data::info = StorageInfo(2, "data");
template<> const StorageInfo Storage<KVPair>::info = StorageInfo(3, "kvpairs");

// Check that the size of the blobs is what we would expect.
#define CHECK_STRUCT_SIZE(type, size) static_assert(sizeof(type) == size, #type " is not " #size " bytes.");
CHECK_STRUCT_SIZE(Trie,   128);
CHECK_STRUCT_SIZE(Element, 36);
CHECK_STRUCT_SIZE(KVPair,  16);
CHECK_STRUCT_SIZE(Pointer,  8);
CHECK_STRUCT_SIZE(Hash,    20);

/**
 * Static pointer, pointing to the root trie.
 */
const Pointer root(Trie::info.type, 0);

/// Used for converting to hexadecimal format
static const char *hex = "0123456789abcdef";

#ifdef NDEBUG
#	define log(...)
#else
/**
 * Used for logging. Has the same type as printf.
 */
static int (*logfn) (const char *, ...) = NULL;
#	define log(...) if (logfn) logfn(__VA_ARGS__)
#endif

/**
 * Changes the method used for logging.
 * @param f, the new logging method with the same signature as printf. Can be NULL to disable logging.
 * @warning If DAGDB has been compiled with NDEBUG defined, it won't write any logging.
 */
void set_log_function(int (*f) (const char *, ...)) {
#ifdef NDEBUG
	if (f) f("Warning: logging is not compiled into current DAGDB library.\n");
#else
	logfn = f;
#endif
}

#define ERROR1(what) std::runtime_error(std::string(what ": ")+strerror(errno))
#define ERROR2(what,param) std::runtime_error(std::string(what " '")+param+"': "+strerror(errno))

/**
 * Reads the item at the given location.
 * @throws runtime_error in case of (io) errors.
 */
template<class T>
void Blob<T>::read(Pointer p) {
	if (pread(p.info().handle, this, sizeof(T), p.address) != sizeof(T))
		throw ERROR2("Failed reading", p.info().name);
}
/**
 * Writes the given item at the given location.
 * @throws runtime_error in case of (io) errors.
 */
template<class T>
void Blob<T>::write(Pointer p) const {
	if (pwrite(p.info().handle, this, sizeof(T), p.address) != sizeof(T))
		throw ERROR2("Failed writing", p.info().name);
}

/**
 * Reads a data element. Allocating memory for the data part.
 */
void Data::read(Pointer p) {
	if (p.type != Data::info.type) throw std::invalid_argument("Reading data using pointer of wrong type.");
	if (pread(info.handle, &length, sizeof(length), p.address) != sizeof(length))
		throw ERROR2("Failed reading", info.name);
	if (data) delete[] data;
	data = new uint8_t[length];
	if ((uint8_t)pread(info.handle, data, length, p.address + sizeof(length)) != length)
		throw ERROR2("Failed reading", info.name);
}

/**
 * Appends a new T to the database.
 */
template<class T>
Pointer Storage<T>::append() const {
	int64_t pos = lseek(info.handle, 0, SEEK_END);
	if (pos == -1) throw ERROR2("Failed to seek", info.name);
	Pointer ptr(info.type, pos);
	Blob<T>::write(ptr);
	log("Appended %s at %Ld\n", info.name, pos);
	return ptr;
}

/**
 * Calculates the offset of a pointer at the given position in the trie.
 */
#define TRIE_POINTER_OFFSET(index) ((int)&(((Trie*)NULL)->pointers[(index)]))

/**
 * Opens the files required by the database. Creating them if they do not yet exist.
 *
 * @param root_dir The root directory of the database. Can be relative to or
 * NULL for current working directory, The directory is created if it does
 * not yet exist.
 * @throws runtime_error in case of (io) errors.
 */
void init(const char *root_dir) {
	int dir;
	int size[storage_size];
	log("Initializing database\n");
	if (root_dir) {
		if (mkdir(root_dir, 0755)) {
			if (errno != EEXIST) {
				throw ERROR2("Can't create dir", root_dir);
			}
			errno = 0;
		} else {
			log("Created directory '%s'\n", root_dir);
		}
		dir = open(root_dir, O_DIRECTORY | O_RDONLY);
		if (dir == -1) {
			throw ERROR2("Failed to open dir", root_dir);
		}
	} else {
		dir = AT_FDCWD;
	}
	for (int i = 0; i < storage_size; i++) {
		if (storage[i] == 0) continue;
		storage[i]->handle = openat(dir, storage[i]->name, O_CREAT | O_RDWR, 0644);
		if (storage[i]->handle == -1) {
			throw ERROR2("Failed to open/create", storage[i]->name);
		}
		int64_t pos = lseek(storage[i]->handle, 0, SEEK_END);
		if (pos == -1)
			throw ERROR2("Failed to seek", storage[i]->name);
		size[i] = pos;
		log("Size of '%s' is %Ld\n", storage[i]->name, pos);
	}
	if (dir != AT_FDCWD) {
		close(dir);
	}
	if (size[Trie::info.type] == 0) {
		Pointer r = Trie().append();
		assert(r == root);
	}
}

/**
 * Removes all contents from the db.
 * @throws runtime_error in case of (io) errors.
 */
void truncate() {
	int i;
	for (i = 0; i < storage_size; i++) {
		if (ftruncate(storage[i]->handle, 0) == -1) throw ERROR2("Failed to truncate file ", storage[i]->name);
	}

	log("Truncated DB\n");
	Pointer r = Trie().append();
	assert(r == root);
}

/**
 * Gets the value of the 'index'-th character of the hexadecimal representation of the hash.
 */
int Hash::nibble(int index) const {
	assert(index >= 0);
	assert(index < 40);
	return (byte[index / 2] >> (1 - index % 2) * 4) & 0xf;
}

/**
 * Attempts to read the hash of the element pointed to by 'p'.
 * If this fails or this element does not have a hash, -1 is returned.
 * The hash is stored in 'h'.
 * Pointers that have a hash are of type DAGDB_ELEMENT or DAGDB_KVPAIR.
 */
// TODO: create tests for KVPair.
Hash::Hash(Pointer p) {
	if (p.type == KVPair::info.type) {
		p.read(p);
		assert(p.type == Element::info.type);
		read(p);
		return;
	} else if (p.type == Element::info.type) {
		read(p);
		return;
	}
	throw std::invalid_argument(buildstring("Pointer does not point to a trie entry."));
}

/**
 * Obtains a reference to the storage info.
 */
const StorageInfo &Pointer::info() const {
	return *storage[type];
}

/**
 * Searches for the element associated by given hash in the trie located at this pointer.
 * @returns Pointer to the element or, if not found, the root pointer.
 * @throws runtime_error in case of (io) errors.
 */
Pointer Pointer::find(Hash h) const {
	int i = 0;
	Trie t;
	Pointer cur = *this;
	log("Searching for %s starting from %p: ", ( {char a[41]; h.write(a); a;}), this);
	while(1) {
		if (cur.type != Trie::info.type) {
			// element found, checking if hash matches.
			Hash hh(cur);

			if(h == hh) {
				log("Found %s at %ld\n", cur.info().name, cur.address);
				return cur;
			} else {
				// hash mismatch, return a NULL pointer (ie. root).
				log("Not found (hash mismatch)\n");
				return root;
			}
		}

		// Navigate one node further in the trie.
		if (i >= 40) break;
		t.read(cur);
		cur = t.pointers[h.nibble(i)];

		if (cur == root) {
			// NULL pointer hit.
			log("Not found (null pointer)\n");
			return root;
		}
		i++;
	}
	// The loop above should never terminate normally.
	assert(0);
	throw std::logic_error("Invalid state");
}

/**
 * Creates a new entry in the map which is pointed to by this pointer.
 * @returns the location where the pointer to the value of the entry must be stored
 * @throws runtime_error in case of (io) errors.
 */
Pointer Pointer::insert(Hash h) const {
	int i = 0;
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
				return cur;
			}
		}

		if (p.type != Trie::info.type) {
			// There is already an element stored at this location. This
			// element might either be the one we are trying to insert
			// (in that case return 1) or a different element, which means
			// we need to create additional trie nodes and move the existing pointer.
			Hash hh(p);

			if(h == hh) {
				// entry already exists in db.
				return cur;
			} else {
				// hash mismatch, create new trie node.
				Pointer nt;
				Trie trie;

				// store the pointer to the old entry.
				trie.pointers[hh.nibble(i)] = p;

				// write the trie to disk
				nt = trie.append();

				// Update the pointer in the current trie.
				nt.write(cur); // corrupts DB if interrupted.

				// Update our navigational pointer to contain the new value at 'root_pointer'.
				p = nt;

				// return to the search algorithm.
			}
		}

		// We are still looking at a trie node. Calculate the address of the next pointer.
		if (i >= 40) break;
		p.address += TRIE_POINTER_OFFSET(h.nibble(i));
		cur = p;
		i++;
	}

	log(" i=%d ", i);
	// The nibbles in the hash are exhausted. There must be something wrong.
	assert(0);
	throw std::logic_error("Invalid state");
}

Pointer::Pointer(uint8_t type, uint64_t address) : type(type), address(address) {
}


/**
 * Calculates the hash of a bytestring.
 */
Hash::Hash(const void *data, int length) {
	gcry_md_hash_buffer(GCRY_MD_SHA1, byte, data, length);
	log("Hashing %ld bytes data from %p: %s\n", length, data, ( {char a[41]; write(a); a;})); 
}

/**
 * Inserts the given data into the root trie of the database.
 * @returns 0 if entry was created successfully, 1 if entry already exists.
 */
// TODO: make more generic?
bool insert_data(const void *data, uint64_t length) {
	if (length >= (1ULL << 56))
		throw std::invalid_argument(buildstring("Data to long: ", length, " < ", (1ULL << 56)));
	int64_t pos;
	Hash h(data, length);

	// Create an entry in our root trie.
	Pointer location = root.insert(h);
	Pointer a;
	a.read(location);
	if (a != root) return false;

	// insert the data in the data file.
	pos = lseek(Data::info.handle, 0, SEEK_END);
	if (pos == -1) throw ERROR1("Failed to seek");
	if (write(Data::info.handle, &length, sizeof(length)) != sizeof(length)) throw ERROR1("Failed to write");
	if (write(Data::info.handle, data, length) != (int64_t)length) throw ERROR1("Failed to write");

	// Create an element for our data.
	Element e;
	e.hash = h;
	e.forward = Pointer(Data::info.type, pos);
	e.reverse = root;
	pos = lseek(Element::info.handle, 0, SEEK_END);
	if (pos == -1) throw ERROR1("Failed to seek");
	Pointer q(Element::info.type, pos);
	e.write(q);

	// Write the pointer to our new entry.
	q.write(location);

	log("Inserted %Ld bytes of data from %p. pointer@%Ld element@%Ld data@%Ld\n", length, data, location.address, q.address, e.forward.address);
	return true;
}

static int value(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	throw std::invalid_argument(buildstring("Invalid character in hexadecimal hash string: '", c, "'"));
}

/**
 * Converts a string of hexadecimal numbers into a hash.
 */
Hash::Hash(const char * t) {
	int i;
	for (i = 0; i < 20; i++) {
		byte[i] = value(t[i * 2]) * 16 + value(t[i * 2 + 1]);
	}
	if (t[40])
		throw std::invalid_argument(buildstring("Hexadecimal hash string is too long"));
}

/**
 * Converts a hash into a string of hexadecimal numbers.
 */
void Hash::write(char *t) const {
	int i;
	for (i = 0; i < 40; i++) {
		t[i] = hex[nibble(i)];
	}
	t[40] = 0;
}

/**
 * @returns the length of the pointed data object or -1 in case of an error.
 */
// TODO: remove in favor of read/write specialization? What about large blobs and random io?
// TODO: Create special data pointer?
uint64_t Pointer::data_length() {
	if (type != Data::info.type) 
		throw std::invalid_argument("Not a data pointer");
	Data d;
	d.read(*this);
	return d.length;
}

template class Blob<Element>;

};
};

