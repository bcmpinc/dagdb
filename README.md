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

File format
-----------
The database stores the following elements:
 
 - A single map hash->data (though multiple is supported as well)
 - Multiple maps hash->map

The following structures are used to store the data:

1. Data element: 
	- hash (20 bytes)
	- reference counter (4 bytes)
	- length (8 bytes)
	- data (length bytes)
2. Map element:  
	- hash (20 bytes) 
	- reference counter (4 bytes)
	- root pointer (8 bytes)
	- size (4 bytes)
3. Treap node:   
	- pointer list (16*8 bytes)

Each is stored in its own file? 
The most significant byte of a pointer is used to determine the file/structure it points to.

Example
-------
There are two maps: one for forward lookups, one for reverse lookups. The following is stored:

	1. {type: application, name: less}
	2. {type: application, name: gcc}

Which results in the following structures being stored. 

	forward map: {
		H("less"): "less"
		H("name"): "name"
		H("type"): "type"
		H("application"): "application"
		H("gcc"): "gcc"
		H(1) -> 1
		H(2) -> 2
	}

	object map (1): {
		H("type"): "application"
		H("name"): "less"
	}

	object map (2): {
		H("type"): "application"
		H("name"): "gcc"
	}

	reverse map: {
		H("application"): 3
		H("less"): 4
		H("gcc"): 5
	}

	reverse key map (3): {
		H("type"): 6
	}

	reverse key map (4): {
		H("name"): 7
	}

	reverse key map (5): {
		H("name"): 8
	}

	reverse object map (6): {
		H(1) -> 1
		H(2) -> 2
	}

	reverse object map (7): {
		H(1) -> 1
	}

	reverse object map (8): {
		H(2) -> 2
	}
