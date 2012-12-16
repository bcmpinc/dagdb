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

#include <gcrypt.h>

#include "api.h"
#include "base.h"

typedef uint8_t dagdb_hash[DAGDB_KEY_LENGTH];
static void dagdb_data_hash(dagdb_hash h, int length, const void * data) {
	gcry_md_hash_buffer(GCRY_MD_SHA1, h, data, length);
}

static dagdb_handle dagdb_record_hash(long int entries, dagdb_record_entry * items) {
	long i;
	dagdb_hash * keylist = malloc(entries * sizeof(dagdb_key) * 2);
	for (i=0; i<entries*2; i++) {
		dagdb_element_key(keylist[i], ((dagdb_pointer*)items)[i]);
	}
	qsort(keylist, entries, sizeof(dagdb_record_entry), NULL);
}

dagdb_handle dagdb_find_data(uint64_t length, const char* data) {
	dagdb_hash h;
	dagdb_data_hash(h, length, data);
	return dagdb_trie_find(dagdb_root(), h);
}

dagdb_handle dagdb_find_record(uint_fast32_t entries, dagdb_record_entry * items)
{

}

