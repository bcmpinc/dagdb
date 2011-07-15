#ifndef DAGDB_BASE_H
#define DAGDB_BASE_H
#include <cstdint>

namespace Dagdb {
	// Storage types and filenames
	enum struct Type : uint8_t {
		trie,
		element,
		data,
		kvpair,
		__max_type,
	};

	// Data structures
	struct Hash;
	struct Pointer;
	struct Element;
	struct Data;
	struct Trie;
	struct KVPair;

	template<class T>
	struct Blob {
		inline bool operator==(const T &b) {return 0==memcmp(this, &b, sizeof(T));}
		inline bool operator!=(const T &b) {return 0!=memcmp(this, &b, sizeof(T));}
		void read(Pointer p); 
		void write(Pointer p) const; 
		Pointer append(Type t) const;
	};

	struct Hash : Blob<Hash> {
		uint8_t byte[20];
		
		inline uint8_t operator[](int i) const {return byte[i];}
		inline uint8_t& operator[](int i) {return byte[i];}
		
		int nibble(int index) const;
		void parse(const char * t);
		void write(char * t) const;
		void compute(const void * data, int length);
	};

	struct Pointer : Blob<Pointer> {
		Type type;
		uint64_t address : 56;
		
		Pointer() = default;
		Pointer(Type type, uint64_t address); 

		// Interogation
		Pointer find(Hash h) const;
		uint64_t data_length();
		
		// Manipulation
		Pointer insert(Hash h) const;
	};

	struct Element : Blob<Element> {
		Hash hash;
		Pointer forward;
		Pointer reverse;
	};

	struct Data : Blob<Data> {
		int64_t length;
		uint8_t data[0];
	};

	struct Trie : Blob<Trie> {
		Pointer pointers[16];
		
		inline Pointer operator[](int i) const {return pointers[i];}
		inline Pointer& operator[](int i) {return pointers[i];}
	};

	struct KVPair{
		Pointer key; // must point to an element.
		Pointer value;
	};
	
	// Global variables & constants
	extern const Pointer root;
	extern const char * filenames[];
	extern const int filename_length;
	extern int storage[(int)Type::__max_type];

	// Initialization
	void set_log_function(int (*f) (const char *,...));
	void init(const char * root_dir);
	void truncate();
	
	bool insert_data(const void * data, uint64_t length);
};


// Data interrogation


#endif
