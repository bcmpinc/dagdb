cmake_minimum_required (VERSION 2.6)
project(dagdb)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Libgcrypt REQUIRED)
find_package(CUnit)

set(lib_src
	src/api.c
	src/bitarray.c
	src/base.c
	src/mem.c
	src/error.c
)

set(test_src
	test/test.c
	test/main.c 
	test/api-test.c
	test/bitarray-test.c
	test/base-test.c
	test/mem-test.c
	test/error-test.c
)

set(rt_src 
	test/test.c
	rigged-test/main-rt.c 
	rigged-test/api-rt.c 
	src/bitarray.c
	src/base.c
	src/mem.c
	src/error.c
)

add_library(dagdb SHARED ${lib_src})
target_link_libraries(dagdb ${LIBGCRYPT_LIBRARIES})
set_property(TARGET dagdb PROPERTY COMPILE_FLAGS "${LIBGCRYPT_CFLAGS} -std=gnu99")

if(CUNIT_FOUND)
	set(valgrind_cmd valgrind --suppressions=${CMAKE_SOURCE_DIR}/valgrind.supp -q --error-exitcode=42)
	set(test_libraries ${CUNIT_LIBRARY} ${LIBGCRYPT_LIBRARIES} --coverage)
	set(test_flags "-I ${CUNIT_INCLUDE_DIR} ${LIBGCRYPT_CFLAGS} --coverage -Wall -Wextra -Wno-unused-parameter -std=gnu99")
	
	# normal tests
	add_executable(dagdb_test ${test_src}) 
	target_link_libraries(dagdb_test ${test_libraries})
	set_property(TARGET dagdb_test PROPERTY COMPILE_FLAGS ${test_flags})
	add_custom_target(run_test ALL ${valgrind_cmd} ./dagdb_test DEPENDS dagdb_test gcov VERBATIM)

	# rigged tests
	add_executable(dagdb_rt ${rt_src}) 
	target_link_libraries(dagdb_rt ${test_libraries})
	set_property(TARGET dagdb_rt PROPERTY COMPILE_FLAGS ${test_flags})
	add_custom_target(run_rt ALL ${valgrind_cmd} ./dagdb_rt DEPENDS dagdb_rt gcov VERBATIM)
	
	# check coverage
	add_custom_command(OUTPUT gcov COMMAND mkdir -p gcov VERBATIM)
	add_custom_target(coverage ALL bash -c "gcov ../CMakeFiles/dagdb_test.dir/test/*.gcno ../CMakeFiles/dagdb_rt.dir/rigged-test/*.gcno > /dev/null" DEPENDS run_test run_rt gcov WORKING_DIRECTORY gcov VERBATIM)
	
else()
	message("Warning: unit tests are not compiled.")
endif()

