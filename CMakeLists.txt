cmake_minimum_required (VERSION 2.6)
project(dagdb)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Libgcrypt REQUIRED)
find_package(UnitTest++)

set(lib_src
    src/base.cpp
    src/base2.c
	src/memory.cpp
)

set(test_src 
	test/main.cpp 
	#test/base.cpp
	test/base2.cpp
	#test/memory.cpp
)

add_library(dagdb SHARED ${lib_src})
target_link_libraries(dagdb ${LIBGCRYPT_LIBRARIES})
set_property(TARGET dagdb PROPERTY COMPILE_FLAGS "-I ${CMAKE_SOURCE_DIR}/include ${LIBGCRYPT_CFLAGS} -std=c++0x")

if(UNITTEST++_FOUND)
	add_executable(dagdb_test ${test_src}) 
	target_link_libraries(dagdb_test dagdb ${UNITTEST++_LIBRARY})
	set_property(TARGET dagdb_test PROPERTY COMPILE_FLAGS "-I ${CMAKE_SOURCE_DIR}/include -I ${CMAKE_SOURCE_DIR}/src -I ${UNITTEST++_INCLUDE_DIR} -std=c++0x " )
	add_custom_target(run_test ALL valgrind --suppressions=${CMAKE_SOURCE_DIR}/valgrind.supp ./dagdb_test DEPENDS dagdb_test VERBATIM)
else()
	message("Warning: unit tests are not compiled.")
endif()

