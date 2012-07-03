cmake_minimum_required (VERSION 2.6)
project(dagdb)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Libgcrypt REQUIRED)
find_package(CUnit)

set(lib_src
	src/bitarray.c
	src/base.c
	src/mem.c
	src/error.c
)

set(test_src 
	test/bitarray-test.c
	test/main.c 
	test/base-test.c
	test/mem-test.c
	test/error-test.c
)

add_library(dagdb SHARED ${lib_src})
target_link_libraries(dagdb ${LIBGCRYPT_LIBRARIES})
set_property(TARGET dagdb PROPERTY COMPILE_FLAGS "${LIBGCRYPT_CFLAGS}")

if(CUNIT_FOUND)
	add_executable(dagdb_test ${test_src}) 
	target_link_libraries(dagdb_test ${CUNIT_LIBRARY} --coverage)
	set_property(TARGET dagdb_test PROPERTY COMPILE_FLAGS "-I ${CUNIT_INCLUDE_DIR} --coverage -Wall " )
	add_custom_command(OUTPUT gcov COMMAND mkdir -p gcov VERBATIM)
	add_custom_target(run_test ALL valgrind --suppressions=${CMAKE_SOURCE_DIR}/valgrind.supp -q --error-exitcode=42 ./dagdb_test DEPENDS dagdb_test gcov VERBATIM)
	get_target_property(ts dagdb_test SOURCES)
	add_custom_command(TARGET run_test POST_BUILD COMMAND bash ARGS -c "for i in ../CMakeFiles/dagdb_test.dir/test/*.o; do gcov -o $i ../x; done" WORKING_DIRECTORY gcov VERBATIM)
else()
	message("Warning: unit tests are not compiled.")
endif()

