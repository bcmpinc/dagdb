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

#include <cstdint>
#include <cstring>

#include "dagdb/memory.h"

namespace dagdb { //
namespace memory { //

class MemData : public interface::Data {
private:
	size_t _length;
	uint8_t *_buffer;
public:
	MemData(const void *buffer, size_t length)
		: _length(length), _buffer(new uint8_t[length]) {
		memcpy(this->_buffer, buffer, length);
	}

	~MemData() {
		delete[] _buffer;
	}

	virtual size_t length() {
		return _length;
	}

	virtual size_t read(void *buffer, size_t offset, size_t length) {
		if (offset < 0 || _length <= offset) 
			return 0;
		if (length > _length - offset)
			length = _length - offset;
		if (length <= 0) 
			return 0;
		memcpy(buffer, _buffer + offset, length);
		return length;
	}
};

typedef const std::map<Element, Element> MemMap;
typedef std::shared_ptr<MemMap> MemMapPtr;

class MemIterator : public interface::RecordIterator {
private:
	MemMapPtr m;
	MemMap::const_iterator it;
public:
	MemIterator(MemMapPtr m) : m(m), it(m->begin()) {
	}
	
	virtual bool next() {
		++it;
		return it != m->end();
	}
	
	virtual dagdb::Element key() {
		if (it == m->end())
			return dagdb::Element();
		return it->first;
	}
	
	virtual dagdb::Element value() {
		if (it == m->end())
			return dagdb::Element();
		return it->second;
	}
};

class MemRecord : public interface::Record {
private:
	MemMapPtr m;
public:
	MemRecord(MemMap &entries) {
		m = MemMapPtr(new MemMap(entries));
	}
	
	virtual RecordIterator iterator() {
		return RecordIterator(new MemIterator(m));
	}
	
	virtual dagdb::Element find(dagdb::Element key) {
		auto it = m->find(key);
		if (it == m->end())
			return dagdb::Element();
		return it->second;
	}
};

Data create_data(const void *buffer, size_t length) {
	return Data(new MemData(buffer, length));
}

Record create_record(MemMap &entries) {
	return Record(new MemRecord(entries));
}


};
};
