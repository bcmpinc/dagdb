cmake_minimum_required (VERSION 2.6)
project(dagdb)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Libgcrypt REQUIRED)
find_package(CUnit)

set(lib_src
	src/base.c
)

set(test_src 
	test/main.c 
	test/base-test.c
)

add_library(dagdb SHARED ${lib_src})
target_link_libraries(dagdb ${LIBGCRYPT_LIBRARIES})
set_property(TARGET dagdb PROPERTY COMPILE_FLAGS "${LIBGCRYPT_CFLAGS}")

if(CUNIT_FOUND)
	add_executable(dagdb_test ${test_src}) 
	target_link_libraries(dagdb_test ${CUNIT_LIBRARY} --coverage)
	set_property(TARGET dagdb_test PROPERTY COMPILE_FLAGS "-I ${CUNIT_INCLUDE_DIR} --coverage -Wall " )
	add_custom_target(run_test ALL valgrind --suppressions=${CMAKE_SOURCE_DIR}/valgrind.supp -q --error-exitcode=42 ./dagdb_test DEPENDS dagdb_test VERBATIM)
else()
	message("Warning: unit tests are not compiled.")
endif()

