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

// include the entire file being tested.
#define DAGDB_HARDEN_MALLOC
#include "../src/mem.c"

#include <sys/stat.h>
#include "test.h"

/**
 * Set or unset the usage flag of the given range in a slab's bitmap.
 */
#define BITMAP_CHECK(s, i, M, v) { dagdb_bitarray m=(M); if ((s->bitmap[(i)]&m)!=m*value) return 0; }
static int check_bitmap_mark(dagdb_pointer location, dagdb_size size, int_fast32_t value) {
	assert(location%S==0);
	assert(size%S==0);
	assert(location%SLAB_SIZE + size <= SLAB_USEABLE_SPACE_SIZE);
	int_fast32_t offset = location & (SLAB_SIZE-1);
	return dagdb_bitarray_check(LOCATE(MemorySlab, location-offset)->bitmap, offset/S, size/S, value);
}

/**
 * Verifies that all linked lists in the free chunk table are properly linked.
 * These lists are cyclic, hence the 'last' element in the list is also the element we start with:
 * the element that is in the free chunk table.
 */
void verify_chunk_table() {
	int_fast32_t i;
	for (i=0; i<CHUNK_TABLE_SIZE; i++) {
		dagdb_pointer list = CHUNK_TABLE_LOCATION(i);
		dagdb_pointer current = list;
		do {
			dagdb_size size = (i==0)?2*S:LOCATE(FreeMemoryChunk, current)->size;
			dagdb_pointer next = LOCATE(FreeMemoryChunk, current)->next;
			CU_ASSERT(current < dagdb_database_size);
			
			// Check that the prev pointer of the next chunk points to current.
			EX_ASSERT_EQUAL_INT(current, LOCATE(FreeMemoryChunk, next)->prev);
			
			if (current>=HEADER_SIZE) {
				// Check that the chunk size is also stored at the end of the chunk
				assert(i==0 || size==*LOCATE(dagdb_size, current + size - S));
				
				// Check that the bitmap matches.
				CU_ASSERT(check_bitmap_mark(current, size,0));
				// There should not be any additional free space left and right of a free chunk.
				if (current%SLAB_SIZE > 0)
					CU_ASSERT(check_bitmap_mark(current-S, S,1));
				if ((current+size)%SLAB_SIZE < SLAB_USEABLE_SPACE_SIZE)
					CU_ASSERT(check_bitmap_mark(current+size, S,1));
			}
			
			// Advance
			current = next;
		} while (current>=HEADER_SIZE);
		EX_ASSERT_EQUAL_INT(current, list);
	}
}

///////////////////////////////////////////////////////////////////////////////

static void print_info() {
	MemorySlab a;
	printf("memory slab: %ld entries, %lub used, %lub bitmap, %ldb wasted\n", BITMAP_SIZE, sizeof(a.data), sizeof(a.bitmap), SLAB_SIZE - sizeof(MemorySlab));
}

static void test_round_up() {
	const dagdb_size L = MIN_CHUNK_SIZE;
	EX_ASSERT_EQUAL_INT(dagdb_round_up(0), L);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(1), L);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(2), L);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(3), L);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(4), L);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(L+0), L);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(L+1), L+S);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(L+2), L+S);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(L+3), L+S);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(L+4), L+S);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(255), 256u);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(256), 256u);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(257), 256u+S);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(258), 256u+S);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(259), 256u+S);
	EX_ASSERT_EQUAL_INT(dagdb_round_up(260), 256u+S);
}

static void test_chunk_id() {
	int i;
	for(i=1; i<10000; i++) {
		if (free_chunk_id(i)==CHUNK_TABLE_SIZE-1) {
			EX_ASSERT_EQUAL_INT(i, MAX_CHUNK_SIZE);
			EX_ASSERT_EQUAL_INT(i%S, 0);
			if (i!=MAX_CHUNK_SIZE) {
				printf("\nMAX_CHUNK_SIZE should be %ld*S\n", i/S);
			}
			break;
		}
		// By allocating a chunk of a given size and then freeing it, we should not be able to increase the id.
		// Additionally, freeing one byte less, should always yield a lower id.
		if (free_chunk_id(i)>=alloc_chunk_id(i+1)) {
			printf("\ni = %d\n", i);
			CU_ASSERT_FALSE_FATAL(free_chunk_id(i)>=alloc_chunk_id(i+1));
		}
		// free_chunk_id must be monotonic.
		if (free_chunk_id(i-1)>free_chunk_id(i)) {
			printf("\ni = %d\n", i);
			CU_ASSERT_FALSE_FATAL(free_chunk_id(i-1)>free_chunk_id(i));
		}
	}
}

static CU_TestInfo test_non_io[] = {
  { "print_info", print_info }, 
  { "round_up", test_round_up },
  { "chunk_id", test_chunk_id },
  CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

static void test_load_init() {
	unlink(DB_FILENAME);
	int r = dagdb_load(DB_FILENAME); EX_ASSERT_NO_ERROR
	CU_ASSERT(r == 0); 
	dagdb_unload();
}

static void test_load_reload() {
	int r = dagdb_load(DB_FILENAME); EX_ASSERT_NO_ERROR
	CU_ASSERT(r == 0); 
	dagdb_unload();
}

static void test_superfluous_unload() {
	dagdb_unload();
}

static void test_load_failure() {
	unlink(DB_FILENAME);
	int r = mkdir(DB_FILENAME,0700);
	CU_ASSERT(r == 0); 
	r = dagdb_load(DB_FILENAME);  
	CU_ASSERT(r == -1); 
	EX_ASSERT_ERROR(DAGDB_ERROR_INVALID_DB); 
	dagdb_unload(); // <- again superfluous
	rmdir(DB_FILENAME);
}

static void test_load_checks() {
	int r;
	Header* h;
	
	// version corruption
	unlink(DB_FILENAME);
	r = dagdb_load(DB_FILENAME); EX_ASSERT_NO_ERROR 
	CU_ASSERT(r == 0); 
	h = LOCATE(Header,0);
	h->format_version=-1; // corrupt header
	dagdb_unload(); 
	r = dagdb_load(DB_FILENAME); EX_ASSERT_ERROR(DAGDB_ERROR_INVALID_DB); 
	CU_ASSERT(r == -1); 
	CU_ASSERT(strstr(dagdb_last_error(), "version")!=NULL);
	dagdb_unload(); // <- again superfluous
	
	// magic corruption
	unlink(DB_FILENAME);
	r = dagdb_load(DB_FILENAME); EX_ASSERT_NO_ERROR 
	CU_ASSERT(r == 0); 
	h = LOCATE(Header,0);
	h->magic=-1; // corrupt header
	dagdb_unload(); 
	r = dagdb_load(DB_FILENAME); EX_ASSERT_ERROR(DAGDB_ERROR_INVALID_DB); 
	CU_ASSERT(r == -1); 
	CU_ASSERT(strstr(dagdb_last_error(), "magic")!=NULL);
	dagdb_unload(); // <- again superfluous

	// root corruption 1
	unlink(DB_FILENAME);
	r = dagdb_load(DB_FILENAME); EX_ASSERT_NO_ERROR 
	CU_ASSERT(r == 0); 
	h = LOCATE(Header,0);
	h->root=DAGDB_TYPE_TRIE; // corrupt header
	dagdb_unload(); 
	r = dagdb_load(DB_FILENAME); EX_ASSERT_ERROR(DAGDB_ERROR_INVALID_DB); 
	CU_ASSERT(r == -1); 
	CU_ASSERT(strstr(dagdb_last_error(), "root")!=NULL);
	dagdb_unload(); // <- again superfluous
	
	// root corruption 1
	unlink(DB_FILENAME);
	r = dagdb_load(DB_FILENAME); EX_ASSERT_NO_ERROR 
	CU_ASSERT(r == 0); 
	h = LOCATE(Header,0);
	h->root=(1ULL<<40) + DAGDB_TYPE_TRIE; // corrupt header
	dagdb_unload(); 
	r = dagdb_load(DB_FILENAME); EX_ASSERT_ERROR(DAGDB_ERROR_INVALID_DB); 
	CU_ASSERT(r == -1); 
	CU_ASSERT(strstr(dagdb_last_error(), "root")!=NULL);
	dagdb_unload(); // <- again superfluous
	
	// root corruption 1
	unlink(DB_FILENAME);
	r = dagdb_load(DB_FILENAME); EX_ASSERT_NO_ERROR 
	CU_ASSERT(r == 0); 
	h = LOCATE(Header,0);
	h->root=512; // corrupt header
	dagdb_unload(); 
	r = dagdb_load(DB_FILENAME); EX_ASSERT_ERROR(DAGDB_ERROR_INVALID_DB); 
	CU_ASSERT(r == -1); 
	CU_ASSERT(strstr(dagdb_last_error(), "root")!=NULL);
	dagdb_unload(); // <- again superfluous
	
	// size corruption
	unlink(DB_FILENAME);
	r = dagdb_load(DB_FILENAME); EX_ASSERT_NO_ERROR 
	CU_ASSERT(r == 0); 
	ftruncate(dagdb_database_fd, 1023);
	dagdb_unload(); 
	r = dagdb_load(DB_FILENAME); EX_ASSERT_ERROR(DAGDB_ERROR_INVALID_DB); 
	CU_ASSERT(r == -1); 
	CU_ASSERT(strstr(dagdb_last_error(), "size")!=NULL);
	dagdb_unload(); // <- again superfluous
	
}

static CU_TestInfo test_loading[] = {
  { "load_init", test_load_init },
  { "load_reload", test_load_reload },
  { "superfluous_unload", test_superfluous_unload },
  { "load_failure", test_load_failure },
  { "load_checks", test_load_checks },
  CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

static void test_mem_initial() {
	EX_ASSERT_EQUAL_INT(dagdb_database_size, SLAB_SIZE);
	verify_chunk_table();
	check_bitmap_mark(0,HEADER_SIZE,1);
	check_bitmap_mark(HEADER_SIZE, SLAB_USEABLE_SPACE_SIZE - HEADER_SIZE,0);
	FreeMemoryChunk * c = LOCATE(FreeMemoryChunk, HEADER_SIZE);
	EX_ASSERT_EQUAL_INT(c->size, SLAB_USEABLE_SPACE_SIZE - HEADER_SIZE);
}

static void test_mem_alloc_too_much() {
	// Checks if malloc returns a null pointer if the requested
	// memory size is too large.
	dagdb_pointer p = dagdb_malloc(MAX_CHUNK_SIZE + 1); 
	EX_ASSERT_EQUAL_INT(p, 0);
	EX_ASSERT_ERROR(DAGDB_ERROR_BAD_ARGUMENT); 
}

typedef struct {
	int oldsize;
	int alloc_size;
	int n;
	dagdb_pointer *p;
} filler;

static void filler_create(filler * f, int alloc_size) {
	EX_ASSERT_NO_ERROR
	f->oldsize = dagdb_database_size;
	f->alloc_size = alloc_size;

	// Assume database is empty.
	EX_ASSERT_EQUAL_INT(LOCATE(FreeMemoryChunk, HEADER_SIZE)->size, SLAB_USEABLE_SPACE_SIZE - HEADER_SIZE);
	
	f->n = (SLAB_USEABLE_SPACE_SIZE - HEADER_SIZE) / f->alloc_size + 1;
	f->p = malloc(f->n * sizeof(dagdb_pointer));
}

static void filler_fill_normal(filler * f) {
	int i;
	// growing
	for (i=0; i<f->n; i++) {
		if (SLAB_USEABLE_SPACE_SIZE - HEADER_SIZE - i*f->alloc_size > 0) {
			// There is free space left in our slab.
			// Check if the size of the remaining chunk, as written on disk, is what we would expect.
			EX_ASSERT_EQUAL_INT(*LOCATE(dagdb_size, SLAB_USEABLE_SPACE_SIZE-S), SLAB_USEABLE_SPACE_SIZE - HEADER_SIZE - i*f->alloc_size);
		} else {
			// There should be no chunks left, so we cannot check the size of the last chunk. 
			// So instead, we check if the chunk table is empty.
			Header* h = LOCATE(Header,0);
			int j;
			for (j=0; j<CHUNK_TABLE_SIZE; j++) {
				EX_ASSERT_EQUAL_INT(h->chunks[j*2  ], CHUNK_TABLE_LOCATION(j));
				EX_ASSERT_EQUAL_INT(h->chunks[j*2+1], CHUNK_TABLE_LOCATION(j));
			}
		}
		f->p[i] = dagdb_malloc(f->alloc_size); EX_ASSERT_NO_ERROR
		
		// Allocation must be successful.
		CU_ASSERT_FATAL(f->p[i]>0);
	}
	EX_ASSERT_EQUAL_INT(dagdb_database_size, f->oldsize + SLAB_SIZE);
	verify_chunk_table();
}

static void filler_resize_reverse(filler * f, dagdb_size newsize) {
	int i;
	// resizing
	for (i=f->n-1; i>=0; i--) {
		dagdb_free(f->p[i], f->alloc_size);
		f->p[i] = dagdb_malloc(newsize); EX_ASSERT_NO_ERROR
		CU_ASSERT_FATAL(f->p[i]>0);
	}
	f->alloc_size = newsize;
	verify_chunk_table();
}

static void filller_destroy(filler * f) {
	free(f->p);
	verify_chunk_table();
}

static void test_mem_growing() {
	filler f;
	filler_create(&f, 2048);
	filler_fill_normal(&f);
	filller_destroy(&f);
	// Test does not cleanup database, hence replace it by a new one.
	close_db(); open_new_db();
}

static void filler_shrink_normal(filler * f) {
	int i;
	for (i=0; i<f->n; i++) {
		dagdb_free(f->p[i], f->alloc_size); 
	}
	EX_ASSERT_EQUAL_INT(dagdb_database_size, f->oldsize);
}

static void filler_shrink_reverse(filler * f) {
	int i;
	for (i=f->n-1; i>=0; i--) {
		dagdb_free(f->p[i], f->alloc_size); 
	}
	EX_ASSERT_EQUAL_INT(dagdb_database_size, f->oldsize);
}

static void test_mem_shrinking() {
	filler f;
	filler_create(&f, 2048);
	filler_fill_normal(&f);
	filler_shrink_normal(&f);
	filller_destroy(&f);
}

static void test_mem_shrinking_reverse() {
	filler f;
	filler_create(&f, 2048);
	filler_fill_normal(&f);
	filler_shrink_reverse(&f);
	filller_destroy(&f);
}

static void test_mem_shrinking_2S() {
	filler f;
	filler_create(&f, 2*S);
	filler_fill_normal(&f);
	filler_shrink_normal(&f);
	filler_fill_normal(&f);
	filler_shrink_reverse(&f);
	filller_destroy(&f);
}

static void test_mem_shorten_chunks() {
	filler f;
	filler_create(&f, 5*S);
	filler_fill_normal(&f);
	filler_resize_reverse(&f, 4*S);
	filler_resize_reverse(&f, 3*S);
	filler_resize_reverse(&f, 2*S);
	filler_shrink_normal(&f);
	filller_destroy(&f);
}

static void test_mem_grow_chunks() {
	filler f;
	filler_create(&f, 5*S);
	filler_fill_normal(&f);
	filler_resize_reverse(&f, 6*S);
	filler_resize_reverse(&f, 7*S);
	filler_resize_reverse(&f, 8*S);
	filler_shrink_normal(&f);
	filller_destroy(&f);
}

static CU_TestInfo test_mem[] = {
  { "memory_initial", test_mem_initial },
  { "memory_error_alloc", test_mem_alloc_too_much },
  { "memory_growing", test_mem_growing },
  { "memory_shrinking", test_mem_shrinking },
  { "memory_shrinking_reverse", test_mem_shrinking_reverse },
  { "memory_shrinking_2S", test_mem_shrinking_2S },
  { "memory_shorten_chunks", test_mem_shorten_chunks },
  { "memory_grow_chunks", test_mem_grow_chunks },
  CU_TEST_INFO_NULL,
};

///////////////////////////////////////////////////////////////////////////////

CU_SuiteInfo mem_suites[] = {
	{ "mem-non-io",   NULL,        NULL,     test_non_io },
	{ "mem-loading",  NULL,        NULL,     test_loading },
	{ "mem-memory",   open_new_db, close_db, test_mem },
	CU_SUITE_INFO_NULL,
};
