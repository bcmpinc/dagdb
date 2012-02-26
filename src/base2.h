/*
    DagDB - A lightweight structured database system.
    Copyright (C) 2012  B.J. Conijn <bcmpinc@users.sourceforge.net>

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

#ifndef DAGDB_BASE2_H
#define DAGDB_BASE2_H
#include <stdint.h>

#define KEY_LENGTH 20
typedef uint8_t dagdb_key[KEY_LENGTH];
typedef unsigned int uint;

// Creation
uint dagdb_create_trie();
uint dagdb_create_element(dagdb_key hash, uint pointer);
uint dagdb_create_data(uint8_t * data);
uint dagdb_create_kvpair(uint key,uint value);

// Destruction
void dagdb_delete_trie(uint location);
void dagdb_delete_element(uint location);
void dagdb_delete_data(uint location);
void dagdb_delete_kvpair(uint location);

// Trie related
int  dagdb_insert(uint trie, uint pointer);
uint dagdb_find(uint trie, dagdb_key hash);
int  dagdb_remove(uint trie, dagdb_key hash);

#endif 
