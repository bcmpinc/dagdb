#ifndef DAGDB_BASE_H
#define DAGDB_BASE_H
#include <cstdint>
#include <cstring>

namespace Dagdb {
	// Data structures
	struct Hash;
	struct Pointer;
	struct Element;
	struct Data;
	struct Trie;
	struct KVPair;

	struct StorageInfo {
		StorageInfo(uint8_t type, const char * name);
		uint8_t type;
		const char * name;
		int handle;
	};
	
	template<class T>
	struct Blob {
		inline Blob() {memset(this, 0, sizeof(T));}
		inline bool operator==(const T &b) {return 0==memcmp(this, &b, sizeof(T));}
		inline bool operator!=(const T &b) {return 0!=memcmp(this, &b, sizeof(T));}
		void read(Pointer p); 
		void write(Pointer p) const; 
	};
	
	template<class T>
	struct Storage : Blob<T> {
		static const StorageInfo info;
		Pointer append() const;
	};

	struct Hash : Blob<Hash> {
		uint8_t byte[20];
		
		Hash() = default;
		Hash(const char * t);
		Hash(const void * data, int length);
		
		inline uint8_t operator[](int i) const {return byte[i];}
		inline uint8_t& operator[](int i) {return byte[i];}
		
		int nibble(int index) const;
		void write(char * t) const;
	};

	struct Pointer : Blob<Pointer> {
		uint8_t type;
		uint64_t address : 56;
		
		Pointer() = default;
		Pointer(uint8_t type, uint64_t address); 
		
		// Interogation
		Pointer find(Hash h) const;
		uint64_t data_length();
		const StorageInfo &info() const;
		
		// Manipulation
		Pointer insert(Hash h) const;
	};

	struct Element : Storage<Element> {
		Hash hash;
		Pointer forward;
		Pointer reverse;
	};

	struct Data {
		static const StorageInfo info;
		uint64_t length;
		uint8_t * data;
		Data() : length(0), data(0) {};
		Data(Data&) = delete;
		~Data() {if (data) delete[] data;};
		inline uint8_t operator[](int i) const {return data[i];}
		inline uint8_t& operator[](int i) {return data[i];}
		void read(Pointer p); 
	};

	struct Trie : Storage<Trie> {
		Pointer pointers[16];
		
		inline Pointer operator[](int i) const {return pointers[i];}
		inline Pointer& operator[](int i) {return pointers[i];}
	};

	struct KVPair : Storage<KVPair> {
		Pointer key; // must point to an element.
		Pointer value;
	};
	
	// Global variables & constants
	extern const Pointer root;

	// Initialization
	void set_log_function(int (*f) (const char *,...));
	void init(const char * root_dir);
	void truncate();
	
	bool insert_data(const void * data, uint64_t length);
};


// Data interrogation


#endif

