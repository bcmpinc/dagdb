DAGDB
=====
A content addressable and structural hash database. 

**DAGDB** is used for storing structured data. We assume that this data has a direct-acyclic-graph structure. 

Requirements
------------
The database will be used for storing:

 - Plain data: reference element -> data
 - Structured data: reference element -> key element -> value element
 - Inverse lookup table for structured data: value element -> key element -> set of reference elements

Requirements for I/O are:

 - Database queries will be read only operations.
 - Database updates will not be failure safe. Hence power failure during an update causes the database to be corrupted. (This might be changed in the future.)

Operational requirements:

 - Insert given plain data as element into the database
 - Insert given record/structured data as element into the database
 - Get value from a given record by given key 
 - Get list of key and value elements entries in a given record
 - Get list of key elements pointing to given element
 - Get list of records pointing to given element using given key

Interface:
----------
For the public api, see api.h.

Warning: the remainder of this section is outdated.

The following data types are exposed as common interface:

 - Type Element: Record or Data

 - Type Data: (String of bytes)
	- int length()
	- Handle open()

 - Type Handle: 
	- int read(*buffer, length)

 - Type Record: (Element -> Element) 
	- Element find(Element)
	- Iterator begin()
	- Iterator end()

 - Type Set: (Element -> Set)
	- Set find(Element)
	- Iterator begin()
	- Iterator end()
	- Set union(Set) [extension]
	- Set intersect(Set) [extension]

 - Type Iterator: (Element -> T) [T in {Element, Set}]
	- bool next()
	- Element key()
	- T value()


File format
-----------
The database stores the following elements:

 - A single map element->data (though multiple is supported as well)
 - Multiple maps element->map

The following structures are used to store the data:

1. Element (Content addressable map entry):
	- hash (20 bytes)
	- padding (4 bytes)
	- forward pointer (8 bytes) to data/treap node
	- reverse pointer (8 bytes) to treap node
2. Data:
	- length (8 bytes)
	- data (length bytes)
3. Trie node (part of a set/map):
	- pointer list (16*8 bytes) to treap node/element
4. Key value map entry:
	- key pointer (8 bytes) to element
	- value pointer (8 bytes) to treap node/element

The database is stored in a single file.
The least significant 3 bytes of a pointer is used to determine what type/structure it points to.
The pointer must be rounded down to a multiple of 8 before use.

Example
-------
There are two maps: one for forward lookups, one for reverse lookups. The following is stored:

1. `s1 = {type: application, name: less}`
2. `s2 = {type: application, name: gcc}`

Which results in the following structures being stored. 

	name = element: { H("name"), data:"name", NULL }
	type = element: { H("type"), data:"type", NULL }
	less = element: { H("less"), data:"less", r_less }
	gcc  = element: { H("gcc"),  data:"gcc",  r_gcc  }
	application = element: { H("application"), data:"application", r_application }
	s1 = element: { H(s1), m1, NULL }
	s2 = element: { H(s2), m2, NULL }

	root = set: {
		name, type, less, gcc, application, s1, s2
	}

	m1 = set: {
		kv: {type -> application}
		kv: {name -> less}
	}

	m2 = set: {
		kv: {type -> application}
		kv: {name -> gcc}
	}

	r_less = set: {
		kv: {name -> set: {s1} }
	}

	r_gcc = set: {
		kv: {name -> set: {s2} }
	}

	r_application = set: {
		kv: {type -> set: {s1, s2} }
	}
