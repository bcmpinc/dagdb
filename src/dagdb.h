#ifndef DAGDB_INTERFACE_H
#define DAGDB_INTERFACE_H

#include <cstdint>
#include <memory>

// This header specifies the various interfaces that can be used by the various 
// modules of this library.

namespace dagdb { //
namespace interface { //

	class Element;
	class Data;
	class Handle;
	class Record;
	class Set;
	class RecordIterator;
	class SetIterator;

};

typedef std::shared_ptr<interface::Element> Element;
typedef std::shared_ptr<interface::Data> Data;
typedef std::shared_ptr<interface::Handle> Handle;
typedef std::shared_ptr<interface::Record> Record;
typedef std::shared_ptr<interface::Set> Set;
typedef std::shared_ptr<interface::RecordIterator> RecordIterator;
typedef std::shared_ptr<interface::SetIterator> SetIterator;

namespace interface { //
	
	// Type Element: Record or Data
	class Element {
	public:
		virtual ~Element() {}
	};

	// Type Data: (String of bytes)
	class Data: public Element {
	public:
		virtual size_t length()=0;
		virtual dagdb::Handle open()=0;
		virtual ~Data() {}
	};
	
	// Type Handle:
	class Handle {
	public:
		virtual size_t read(void * buffer, size_t length)=0;
		virtual ~Handle() {}
	};

	// Type Record: (Element -> Element) 
	class Record : public Element {
	public:
		virtual dagdb::Element find(dagdb::Element key)=0;
		virtual dagdb::RecordIterator begin()=0;
		virtual dagdb::RecordIterator end()=0;
		virtual ~Record() {}
	};
	
	// Type Set: (Element -> Set)
	class Set {
	public:
		virtual dagdb::Set find(dagdb::Element)=0;
		virtual dagdb::SetIterator begin()=0;
		virtual dagdb::SetIterator end()=0;
		virtual ~Set() {}
	};
	
	// Type Iterator: (Element -> T) [T in {Element, Set}]
	class RecordIterator {
	public:
		virtual bool next()=0;
		virtual dagdb::Element key()=0;
		virtual dagdb::Element value()=0;
		virtual ~RecordIterator() {}
	};
	class SetIterator {
	public:
		virtual bool next()=0;
		virtual dagdb::Element key()=0;
		virtual dagdb::Set value()=0;
		virtual ~SetIterator() {}
	};

};
};

#endif
