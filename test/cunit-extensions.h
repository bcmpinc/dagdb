#ifndef DAGDB_TEST_CUNIT_H
#define DAGDB_TEST_CUNIT_H
#include <CUnit/CUnit.h>
#define EX_ASSERT_EQUAL(actual, expected, type, fmt, name) { \
	type a = (actual), e = (expected); \
	char msg[512];\
	snprintf(msg,512, #name "(%s, %s) = " #name "("fmt", "fmt")", #actual, #expected, a, e); \
	CU_assertImplementation(a==e, __LINE__, msg, __FILE__, "", CU_FALSE); \
}
#define EX_ASSERT_EQUAL_INT(actual, expected) EX_ASSERT_EQUAL(actual, expected, int, "%d", EX_ASSERT_EQUAL_INT)
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
#endif
