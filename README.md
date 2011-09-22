DAGDB
=====
A content addressable and structural hash database. 

**DAGDB** is used for storing structured data. We assume that this data has a direct-acyclic-graph structure. 

Requirements
------------
The database will be used for storing:

 - Plain data: reference hash -> data
 - Structured data: reference hash -> key hash -> value hash 
 - Inverse lookup table for structured data: value hash -> key hash -> reference hash set

Requirements for I/O are:

 - Database queries will be read only operations.
 - Database updates will not be failure safe. Hence power failure during an update causes the database to be corrupted.

Operational requirements:

 - Insert given plain data into a given map
 - Insert given record/structured data into a given map
 - Get entry from a given map by given hash
 - Get list of entries in a given map
 - Get list of key hashes pointing to given data
 - Get list of records pointing to given data using given key

Interface:
----------
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

 - A single map hash->data (though multiple is supported as well)
 - Multiple maps hash->map

The following structures are used to store the data:

1. Element (Content addressable map entry):
	- hash (20 bytes)
	- forward pointer (8 bytes) to data/treap node
	- reverse pointer (8 bytes) to treap node
2. Data:
	- length (8 bytes)
	- data (length bytes)
3. Treap node (part of a set/map):
	- pointer list (16*8 bytes) to treap node/element
4. Key value map entry:
	- key pointer (8 bytes) to element
	- value pointer (8 bytes) to treap node/element

Each is stored in its own file to allow the creation of repair tools.
The most significant byte of a pointer is used to determine the file/structure it points to.

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
