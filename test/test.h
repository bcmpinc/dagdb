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

#ifndef DAGDB_TEST_H
#define DAGDB_TEST_H

#include <CUnit/CUnit.h>
#include <stdlib.h>
#include <stdio.h>

// Equality checking
#define EX_ASSERT_EQUAL(actual, expected, type, fmt, name) { \
	type a = (actual), e = (expected); \
	char msg[512];\
	snprintf(msg,512, #name "(%s, %s) = " #name "("fmt", "fmt")", #actual, #expected, a, e); \
	CU_assertImplementation(a==e, __LINE__, msg, __FILE__, "", CU_FALSE); \
}
#define EX_ASSERT_EQUAL_INT(actual, expected) EX_ASSERT_EQUAL(actual, expected, int, "%d", EX_ASSERT_EQUAL_INT)
#define EX_ASSERT_EQUAL_LONG_HEX(actual, expected) EX_ASSERT_EQUAL(actual, expected, uint64_t, "%lx", EX_ASSERT_EQUAL_LONG_HEX)

#define EX_ASSERT_EQUAL_STRING(actual, expected) { \
	char msg[512];\
	snprintf(msg,512, "EX_ASSERT_EQUAL_BYTEARRAY(%s, %s) = EX_ASSERT_EQUAL_BYTEARRAY(%.20s%s, %.20s%s)", #actual, #expected, actual, (strlen(actual)>20?"...":""), expected, (strlen(expected)>20?"...":"")); \
	CU_assertImplementation(strcmp(actual, expected)==0, __LINE__, msg, __FILE__, "", CU_FALSE); \
}

// Error checking
#define EX_ASSERT_ERROR(expected) {\
	char msg[512];\
	snprintf(msg,512, "CHECK_ERROR(%s), got %d: %s", #expected, dagdb_errno, dagdb_last_error()); \
	CU_assertImplementation(dagdb_errno==(expected), __LINE__, msg, __FILE__, "", CU_FALSE); \
	dagdb_errno=DAGDB_ERROR_NONE; \
}
#define EX_ASSERT_NO_ERROR {\
	char msg[512];\
	snprintf(msg,512, "EX_ASSERT_NO_ERROR, got %d: %s", dagdb_errno, dagdb_last_error()); \
	CU_assertImplementation(dagdb_errno==DAGDB_ERROR_NONE, __LINE__, msg, __FILE__, "", CU_FALSE); \
	dagdb_errno=DAGDB_ERROR_NONE; \
}

// Helper functions
int open_new_db();
int close_db();
void verify_chunk_table();
#endif 
