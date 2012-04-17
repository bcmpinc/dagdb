#include <CUnit/CUnit.h>
#include <string.h>

// include the entire file being tested.
#include <base2.c>

/** \file
 * Tests the I/O base system of DagDB.
 * Note that, contrary to common unit testing practice,
 * the order of the tests is important.
 * For example: tests performing reads and writes must be between a
 * test that opens the database and a test that closes it.
 * This means that if one test fails, subsequent tests are more likely to
 * fail as well.
 * Also, tests are listed in increasing complexity, such that later tests
 * can rely on functionality in previous tests to be correct, but are not
 * affected by bugs that can trigger later tests.
 */


static void test_round_up() {
	CU_ASSERT_EQUAL(dagdb_round_up(0), 0u);
	CU_ASSERT_EQUAL(dagdb_round_up(1), 4u);
	CU_ASSERT_EQUAL(dagdb_round_up(2), 4u);
	CU_ASSERT_EQUAL(dagdb_round_up(3), 4u);
	CU_ASSERT_EQUAL(dagdb_round_up(4), 4u);
	CU_ASSERT_EQUAL(dagdb_round_up(255), 256u);
	CU_ASSERT_EQUAL(dagdb_round_up(256), 256u);
	CU_ASSERT_EQUAL(dagdb_round_up(257), 260u);
	CU_ASSERT_EQUAL(dagdb_round_up(258), 260u);
	CU_ASSERT_EQUAL(dagdb_round_up(259), 260u);
	CU_ASSERT_EQUAL(dagdb_round_up(260), 260u);
}

static CU_TestInfo test_non_io[] = {
  { "round_up", test_round_up },
  CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////
#define DB_FILENAME "test.dagdb"

static void test_load_init() {
	unlink(DB_FILENAME);
	int r = dagdb_load(DB_FILENAME);
	CU_ASSERT(r == 0);
	dagdb_unload();
}

static void test_load_reload() {
	int r = dagdb_load(DB_FILENAME);
	CU_ASSERT(r == 0);
	dagdb_unload();
}

static void test_superfluous_unload() {
	dagdb_unload();
}

static CU_TestInfo test_loading[] = {
  { "load_init", test_load_init },
  { "load_reload", test_load_reload },
  { "superfluous_unload", test_superfluous_unload },
  CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

// The keys are 1 byte too short, as the last byte is filled by the 0-termination character.
static dagdb_key key1 = {"0123456789012345678"};
static dagdb_key key2 = {"0123056789012345678"};
static dagdb_key key3 = {"0123456789012345670"};
static dagdb_key key4 = {"1123456789012345670"};

static void test_data() {
	const char *data = "This is a test";
	uint len = strlen(data);
	dagdb_pointer p = dagdb_data_create(len, data);
	CU_ASSERT_EQUAL(dagdb_get_type(p), DAGDB_TYPE_DATA);
	CU_ASSERT_EQUAL(dagdb_data_length(p), len);
	CU_ASSERT_NSTRING_EQUAL((const char *) dagdb_data_read(p), data, len);
	dagdb_data_delete(p);
	CU_ASSERT_EQUAL(dagdb_data_length(p), 0u); // UNDEFINED BEHAVIOR
}

static void test_element() {
	dagdb_pointer el = dagdb_element_create(key1, 1000, 1337);
	CU_ASSERT(el);
	CU_ASSERT_EQUAL(dagdb_get_type(el), DAGDB_TYPE_ELEMENT);
	CU_ASSERT_EQUAL(dagdb_element_data(el), 1000u);
	CU_ASSERT_EQUAL(dagdb_element_backref(el), 1337u);
	dagdb_element_delete(el);
}

static void test_kvpair() {
	// Depends on element
	dagdb_pointer el = dagdb_element_create(key1, 1, 2);
	CU_ASSERT_EQUAL(dagdb_get_type(el), DAGDB_TYPE_ELEMENT);
	dagdb_pointer kv = dagdb_kvpair_create(el, 42);
	CU_ASSERT(kv);
	CU_ASSERT_EQUAL(dagdb_get_type(kv), DAGDB_TYPE_KVPAIR);
	CU_ASSERT_EQUAL(dagdb_kvpair_key(kv), el);
	CU_ASSERT_EQUAL(dagdb_kvpair_value(kv), 42u);
	dagdb_kvpair_delete(kv);
	CU_ASSERT_EQUAL(dagdb_element_data(el), 1u);
	CU_ASSERT_EQUAL(dagdb_element_backref(el), 2u);
	dagdb_element_delete(el);
}

static void test_trie() {
	dagdb_pointer t = dagdb_trie_create();
	CU_ASSERT(t);
	CU_ASSERT_EQUAL(dagdb_get_type(t), DAGDB_TYPE_TRIE);
	dagdb_trie_delete(t);
}

static CU_TestInfo test_basic_io[] = {
  { "data", test_data },
  { "element", test_element },
  { "kvpair", test_kvpair },
  { "trie", test_trie },
  CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

static void test_insert() {
	int r;
	dagdb_pointer el1 = dagdb_element_create(key1, 1, 2);
	dagdb_pointer el2 = dagdb_element_create(key2, 3, 4);
	dagdb_pointer el3 = dagdb_element_create(key1, 5, 6); // this is indeed the first key.
	CU_ASSERT_EQUAL(dagdb_get_type(el1), DAGDB_TYPE_ELEMENT);
	CU_ASSERT_EQUAL(dagdb_get_type(el2), DAGDB_TYPE_ELEMENT);
	CU_ASSERT_EQUAL(dagdb_get_type(el3), DAGDB_TYPE_ELEMENT);
	r = dagdb_trie_insert(dagdb_root(), el1);
	CU_ASSERT_EQUAL(r, 1);
	r = dagdb_trie_insert(dagdb_root(), el2);
	CU_ASSERT_EQUAL(r, 1);
	r = dagdb_trie_insert(dagdb_root(), el3); // checking duplicate inserts.
	CU_ASSERT_EQUAL(r, 0);
}

static void test_find() {
	dagdb_pointer el1 = dagdb_trie_find(dagdb_root(), key1);
	dagdb_pointer el2 = dagdb_trie_find(dagdb_root(), key2);
	CU_ASSERT_EQUAL(dagdb_get_type(el1), DAGDB_TYPE_ELEMENT);
	CU_ASSERT_EQUAL(dagdb_get_type(el2), DAGDB_TYPE_ELEMENT);
	CU_ASSERT_EQUAL(dagdb_element_data(el1), 1u);
	CU_ASSERT_EQUAL(dagdb_element_backref(el1), 2u);
	CU_ASSERT_EQUAL(dagdb_element_data(el2), 3u);
	CU_ASSERT_EQUAL(dagdb_element_backref(el2), 4u);
	CU_ASSERT_EQUAL(dagdb_trie_find(dagdb_root(), key3), 0u); // fail on empty spot
	CU_ASSERT_EQUAL(dagdb_trie_find(dagdb_root(), key4), 0u); // fail on key mismatch.
}

static void test_remove() {
	int r;
	r = dagdb_trie_remove(dagdb_root(), key1);
	CU_ASSERT_EQUAL(r, 1);
	r = dagdb_trie_remove(dagdb_root(), key1);
	CU_ASSERT_EQUAL(r, 0);
	r = dagdb_trie_remove(dagdb_root(), key3);
	CU_ASSERT_EQUAL(r, 0);
	r = dagdb_trie_remove(dagdb_root(), key4);
	CU_ASSERT_EQUAL(r, 0);

	dagdb_pointer el1 = dagdb_trie_find(dagdb_root(), key1);
	CU_ASSERT_EQUAL(el1, 0u);

	dagdb_pointer el2 = dagdb_trie_find(dagdb_root(), key2);
	CU_ASSERT_EQUAL(dagdb_get_type(el2), DAGDB_TYPE_ELEMENT);
	CU_ASSERT_EQUAL(dagdb_element_data(el2), 3u);
	CU_ASSERT_EQUAL(dagdb_element_backref(el2), 4u);

	r = dagdb_trie_remove(dagdb_root(), key2);
	CU_ASSERT_EQUAL(r, 1);
	el2 = dagdb_trie_find(dagdb_root(), key2);
	CU_ASSERT_EQUAL(el2, 0u);
}

static void test_trie_kvpair() {
	int r;
	dagdb_pointer el = dagdb_element_create(key1, 1, 2);
	CU_ASSERT_EQUAL(dagdb_get_type(el), DAGDB_TYPE_ELEMENT);
	dagdb_pointer kv = dagdb_kvpair_create(el, 3);
	CU_ASSERT_EQUAL(dagdb_get_type(kv), DAGDB_TYPE_KVPAIR);
	
	r = dagdb_trie_insert(dagdb_root(), kv);
	CU_ASSERT_EQUAL(r, 1);
	r = dagdb_trie_insert(dagdb_root(), el);
	CU_ASSERT_EQUAL(r, 0);

	CU_ASSERT_EQUAL(dagdb_trie_find(dagdb_root(), key1), kv);

	r = dagdb_trie_remove(dagdb_root(), key1);
	CU_ASSERT_EQUAL(r, 1);
	CU_ASSERT_EQUAL(dagdb_kvpair_key(kv), el);
	CU_ASSERT_EQUAL(dagdb_kvpair_value(kv), 3u);
	dagdb_element_delete(el);
}

static CU_TestInfo test_trie_io[] = {
  { "insert", test_insert },
  { "find", test_find },
  { "remove", test_remove },
  { "kvpair", test_trie_kvpair },
  CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

static int open_new_db() {
	unlink(DB_FILENAME);
	return dagdb_load(DB_FILENAME);
}

static int close_db() {
	dagdb_unload();
	return 0;
}

CU_SuiteInfo base2_suites[] = {
	{ "base-non-io",   NULL,        NULL,     test_non_io },
	{ "base-loading",  NULL,        NULL,     test_loading },
	{ "base-basic-io", open_new_db, close_db, test_basic_io },
	{ "base-trie-io",  open_new_db, close_db, test_trie_io },
	CU_SUITE_INFO_NULL,
};
