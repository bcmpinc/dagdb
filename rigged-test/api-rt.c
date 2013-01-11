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

// rigs
#include "../src/types.h"

static int rig_dagdb_trie_insert=0;
int dagdb_trie_insert(dagdb_pointer trie, dagdb_pointer pointer);
int dagdb_trie_insert_rigged(dagdb_pointer trie, dagdb_pointer pointer) {
	if (rig_dagdb_trie_insert)
		return -1;
	else
		return dagdb_trie_insert(trie, pointer);
}

#define dagdb_trie_insert dagdb_trie_insert_rigged


// include the entire file being tested.
#include "../src/api.c"

#include "../test/test.h"

static void test_data_write() {
	char * text = "Just some text";
	int len = strlen(text);
	rig_dagdb_trie_insert = 1;
	dagdb_handle e = dagdb_find_bytes(len, text);
	dagdb_handle h = dagdb_write_bytes(len, text);
	dagdb_handle f = dagdb_find_bytes(len, text);
	EX_ASSERT_EQUAL_INT(e,0);
	EX_ASSERT_EQUAL_INT(h, 0);
	EX_ASSERT_EQUAL_INT(f,0);
	rig_dagdb_trie_insert = 0;
}

static void test_record_write() {
	dagdb_handle refs[10];
	int i;
	for(i=0; i<10; i++) {
		refs[i] = dagdb_write_bytes(1, "abcdefghij" + i);
		CU_ASSERT(refs[i] > 0);
	}
	
	rig_dagdb_trie_insert = 1;
	dagdb_handle ref0 = dagdb_find_record(5, (dagdb_record_entry*)refs);
	EX_ASSERT_EQUAL_INT(ref0, 0);
	
	dagdb_handle ref1 = dagdb_write_record(5, (dagdb_record_entry*)refs);
	EX_ASSERT_EQUAL_INT(ref1, 0);

	dagdb_handle ref2 = dagdb_find_record(5, (dagdb_record_entry*)refs);
	EX_ASSERT_EQUAL_INT(ref2, 0);
	
	rig_dagdb_trie_insert = 0;
}

static CU_TestInfo tests[] = {
  { "data_write_fail", test_data_write },
  { "record_write_fail", test_record_write },
  CU_TEST_INFO_NULL,
};

CU_SuiteInfo api_rt1_suites[] = {
	{ "api-rt1",     open_new_db, close_db,   tests },
	CU_SUITE_INFO_NULL,
};
