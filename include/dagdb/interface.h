/*
    DagDB - A lightweight structured database system.
    Copyright (C) 2011  B.J. Conijn <bcmpinc@sourceforge.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DAGDB_INTERFACE_H
#define DAGDB_INTERFACE_H

// Provides 'shared_ptr', which is used for memory management.
#include <memory>

/** @file
 * @brief Specifies the interfaces that form the basis of the DagDB library.
 * 
 */

namespace dagdb { //
namespace interface { //

	class Element;
	class Data;
	class Record;
	class Set;
	class RecordIterator;
	class SetIterator;

};

typedef std::shared_ptr<interface::Element> Element;
typedef std::shared_ptr<interface::Data> Data;
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
		virtual size_t read(void * buffer, size_t offset, size_t length)=0;
		virtual ~Data() {}
	};
	
	// Type Record: (Element -> Element) 
	class Record : public Element {
	public:
		virtual dagdb::Element find(dagdb::Element key)=0;
		virtual dagdb::RecordIterator iterator()=0;
		virtual ~Record() {}
	};
	
	// Type Set: (Element -> Set)
	class Set {
	public:
		virtual dagdb::Set find(dagdb::Element)=0;
		virtual dagdb::SetIterator iterator()=0;
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
